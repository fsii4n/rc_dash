#include "rc_monitor.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

// ---- protocol constants ----------------------------------------------------

#define CMD_TYPE_REMOVE_ALL 0
#define CMD_TYPE_REMOVE 1
#define CMD_TYPE_ADD_INCOMPLETE 2
#define CMD_TYPE_ADD 3

#define CMD_RESULT_OK 0
#define CMD_RESULT_PAYLOAD_OUT_OF_SEQUENCE 1
#define CMD_RESULT_EQUATION_EXCEPTION 2

#define MAX_PAYLOAD_PART 17

static const uint16_t MAIN_SERVICE_UUID = 0x1ff8;
static const uint16_t CONFIG_CHAR_UUID = 0x05;
static const uint16_t NOTIFY_CHAR_UUID = 0x06;

struct MonitorDef {
  const char *equation;
};

// RaceChrono channel equations. Values arrive as int32; equations multiply by
// 10 where a decimal is needed (undo on the display side).
static const MonitorDef kMonitors[RC_CH_COUNT] = {
    {"channel(device(gps), speed)*10.0"},
    {"channel(device(lap), lap_number)"},
    {"channel(device(lap), lap_time)*10.0"},
    {"channel(device(lap), previous_lap_time)*10.0"},
    {"channel(device(lap), best_lap_time)*10.0"},
    // Invalid until a comparison lap exists (first full lap of the session)
    {"channel(device(lap), delta_lap_time)*100.0"},
};

// ---- shared state ----------------------------------------------------------

static portMUX_TYPE sLock = portMUX_INITIALIZER_UNLOCKED;
static int32_t sValues[RC_CH_COUNT];
static volatile RcState sState = RC_STATE_ADVERTISING;
static volatile uint32_t sLastDataMs = 0;
static volatile bool sConnected = false;

static NimBLEServer *sServer = nullptr;
static NimBLECharacteristic *sConfigChar = nullptr;
static NimBLECharacteristic *sNotifyChar = nullptr;

static void resetValues() {
  portENTER_CRITICAL(&sLock);
  for (int i = 0; i < RC_CH_COUNT; i++) sValues[i] = RC_INVALID_VALUE;
  sLastDataMs = 0;
  portEXIT_CRITICAL(&sLock);
}

void rcMonitorGet(RcSnapshot &out) {
  portENTER_CRITICAL(&sLock);
  for (int i = 0; i < RC_CH_COUNT; i++) out.values[i] = sValues[i];
  out.lastDataMs = sLastDataMs;
  portEXIT_CRITICAL(&sLock);
  out.state = sState;
}

// ---- BLE callbacks ---------------------------------------------------------

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
    Serial.println("[rc] central connected");
    resetValues();
    sConnected = true;
    sState = RC_STATE_CONFIGURING;
  }
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo,
                    int reason) override {
    Serial.printf("[rc] disconnected (reason %d)\n", reason);
    sConnected = false;
    sState = RC_STATE_ADVERTISING;
    NimBLEDevice::startAdvertising();
  }
};

// Last command result written back by the central; configureMonitors()
// waits on these so a dropped indication part gets detected and resent.
static volatile int sLastAckId = -1;
static volatile int sLastAckResult = -1;

class ConfigCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *chr, NimBLEConnInfo &connInfo) override {
    NimBLEAttValue v = chr->getValue();
    if (v.length() < 2) return;
    switch (v[0]) {
      case CMD_RESULT_PAYLOAD_OUT_OF_SEQUENCE:
        Serial.printf("[rc] monitor %d payload out of sequence\n", v[1]);
        break;
      case CMD_RESULT_EQUATION_EXCEPTION:
        Serial.printf("[rc] monitor %d equation exception\n", v[1]);
        break;
      default:
        break;
    }
    sLastAckResult = v[0];
    sLastAckId = v[1];
  }
};

class NotifyCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *chr, NimBLEConnInfo &connInfo) override {
    NimBLEAttValue v = chr->getValue();
    const uint8_t *d = v.data();
    size_t len = v.length();
    uint32_t now = millis();
    static uint32_t sWriteCount = 0;
    if (sWriteCount++ % 50 == 0) {
      Serial.printf("[rc] data write #%lu, %u bytes:", (unsigned long)sWriteCount,
                    (unsigned)len);
      for (size_t i = 0; i < len && i < 30; i++) Serial.printf(" %02x", d[i]);
      Serial.println();
    }
    portENTER_CRITICAL(&sLock);
    for (size_t pos = 0; pos + 5 <= len; pos += 5) {
      uint8_t id = d[pos];
      int32_t value = (int32_t)((uint32_t)d[pos + 1] << 24 |
                                (uint32_t)d[pos + 2] << 16 |
                                (uint32_t)d[pos + 3] << 8 | (uint32_t)d[pos + 4]);
      if (id < RC_CH_COUNT) sValues[id] = value;
    }
    sLastDataMs = now;
    portEXIT_CRITICAL(&sLock);
    if (sState == RC_STATE_CONFIGURING) sState = RC_STATE_STREAMING;
  }
};

static ServerCallbacks sServerCb;
static ConfigCharCallbacks sConfigCb;
static NotifyCharCallbacks sNotifyCb;

// ---- monitor configuration handshake ---------------------------------------

// Sends one config command over the indicate characteristic. Long equations
// are split into 17-byte parts (ADD_INCOMPLETE ... ADD sequence).
static bool sendConfigCommand(int cmdType, int monitorId, const char *payload) {
  int payloadSequence = 0;
  size_t remaining = payload ? strlen(payload) : 0;
  const char *p = payload;

  do {
    size_t part = remaining > MAX_PAYLOAD_PART ? MAX_PAYLOAD_PART : remaining;
    uint8_t bytes[3 + MAX_PAYLOAD_PART];
    bytes[0] = (uint8_t)((remaining > part) ? CMD_TYPE_ADD_INCOMPLETE : cmdType);
    bytes[1] = (uint8_t)monitorId;
    bytes[2] = (uint8_t)payloadSequence;
    if (part) memcpy(bytes + 3, p, part);

    if (!sConnected) return false;
    sConfigChar->setValue(bytes, 3 + part);
    if (!sConfigChar->indicate()) return false;
    vTaskDelay(pdMS_TO_TICKS(50));

    p += part;
    remaining -= part;
    payloadSequence++;
  } while (remaining > 0);

  return true;
}

// Sends one ADD and waits for the central's result write. Indications can
// drop a payload part (seen with both Android and macOS centrals); without
// the ack-wait the equation registers corrupted and the channel goes dead.
static bool addMonitorAcked(int monitorId) {
  for (int attempt = 0; attempt < 3; attempt++) {
    sLastAckId = -1;
    if (!sendConfigCommand(CMD_TYPE_ADD, monitorId,
                           kMonitors[monitorId].equation)) {
      return false;
    }
    for (int waited = 0; waited < 1000; waited += 10) {
      if (sLastAckId == monitorId) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (sLastAckId == monitorId && sLastAckResult == CMD_RESULT_OK) return true;
    Serial.printf("[rc] monitor %d %s, resending\n", monitorId,
                  sLastAckId == monitorId ? "rejected" : "not acked");
  }
  return false;
}

static bool configureMonitors() {
  if (!sendConfigCommand(CMD_TYPE_REMOVE_ALL, 0, nullptr)) return false;
  for (int i = 0; i < RC_CH_COUNT; i++) {
    if (!addMonitorAcked(i)) return false;
  }
  return true;
}

// Drives the configure-on-connect state machine.
static void rcTask(void *arg) {
  bool configured = false;
  for (;;) {
    if (!sConnected) {
      configured = false;
    } else if (!configured) {
      // Give the central time to discover services and subscribe.
      vTaskDelay(pdMS_TO_TICKS(1500));
      if (sConnected) {
        Serial.println("[rc] configuring monitors...");
        if (configureMonitors()) {
          Serial.println("[rc] monitors configured");
          configured = true;
        } else {
          Serial.println("[rc] configure failed, retrying");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// ---- setup -----------------------------------------------------------------

void rcMonitorStart() {
  resetValues();

  NimBLEDevice::init("RC DIY");
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  sServer = NimBLEDevice::createServer();
  sServer->setCallbacks(&sServerCb);
  sServer->advertiseOnDisconnect(true);

  NimBLEService *service = sServer->createService(MAIN_SERVICE_UUID);
  sConfigChar = service->createCharacteristic(
      CONFIG_CHAR_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
  sConfigChar->setCallbacks(&sConfigCb);
  sNotifyChar = service->createCharacteristic(NOTIFY_CHAR_UUID,
                                              NIMBLE_PROPERTY::WRITE_NR);
  sNotifyChar->setCallbacks(&sNotifyCb);
  service->start();

  // RaceChrono lists DIY devices by the advertised 0x1ff8 service; the
  // "RC DIY #xxxx" name convention makes multiple devices distinguishable.
  std::string addr = NimBLEDevice::getAddress().toString();  // "aa:bb:cc:dd:ee:ff"
  char name[32];
  snprintf(name, sizeof(name), "RC DIY #%c%c%c%c", toupper(addr[12]),
           toupper(addr[13]), toupper(addr[15]), toupper(addr[16]));

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(name);
  adv->addServiceUUID(service->getUUID());
  adv->setMinInterval(160);
  adv->setMaxInterval(160);
  adv->enableScanResponse(true);
  adv->start();
  Serial.printf("[rc] advertising as %s\n", name);

  xTaskCreatePinnedToCore(rcTask, "rc", 4096, nullptr, 3, nullptr, 0);
}
