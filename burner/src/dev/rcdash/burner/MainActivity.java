package dev.rcdash.burner;

// Throwaway test rig that plays RaceChrono's role against the rc_dash firmware:
// scans for the "RC DIY" peripheral (service 0x1ff8), subscribes to the config
// characteristic (0x05) indications, acks the monitor ADD commands, then
// streams simulated channel values (10 Hz) to the data characteristic (0x06)
// in the same packed {uint8 id, int32 big-endian} format RaceChrono uses.

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.UUID;

public class MainActivity extends Activity {

    private static final UUID SERVICE_UUID =
            UUID.fromString("00001ff8-0000-1000-8000-00805f9b34fb");
    private static final UUID CONFIG_CHAR_UUID =
            UUID.fromString("00000005-0000-1000-8000-00805f9b34fb");
    private static final UUID NOTIFY_CHAR_UUID =
            UUID.fromString("00000006-0000-1000-8000-00805f9b34fb");
    private static final UUID CCCD_UUID =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private static final int CMD_TYPE_REMOVE_ALL = 0;
    private static final int CMD_TYPE_REMOVE = 1;
    private static final int CMD_TYPE_ADD_INCOMPLETE = 2;
    private static final int CMD_TYPE_ADD = 3;
    private static final int CMD_RESULT_OK = 0;

    private static final int PERMISSION_REQ = 1;
    private static final int SIM_PERIOD_MS = 100; // 10 Hz like a real GPS feed

    private final Handler handler = new Handler(Looper.getMainLooper());

    private TextView statusView;
    private TextView telemetryView;
    private TextView logView;
    private Button actionButton;

    private BluetoothLeScanner scanner;
    private ScanCallback scanCallback;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic configChar;
    private BluetoothGattCharacteristic notifyChar;
    private int mtuPayload = 20; // ATT default (23) minus 3-byte header

    // monitorId -> equation string, as configured by the firmware
    private final List<String> equations =
            Collections.synchronizedList(new ArrayList<>());
    private final StringBuilder pendingEquation = new StringBuilder();

    private boolean simRunning = false;
    private long simStartMs;
    private long lapStartMs;
    private int lapNumber = 1;
    private int prevLapDeci = 0x7fffffff;
    private int bestLapDeci = 0x7fffffff;
    private long lapDurationMs = 19500; // first simulated lap: 19.5s (kart-ish)
    private int deltaCs = 0;            // simulated delta, centiseconds

    // ---- UI -----------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (16 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad * 2, pad, pad);
        root.setBackgroundColor(Color.BLACK);

        statusView = new TextView(this);
        statusView.setTextColor(Color.WHITE);
        statusView.setTextSize(18);
        statusView.setTypeface(Typeface.DEFAULT_BOLD);
        statusView.setText("RC Burner 0.2 — idle");
        root.addView(statusView);

        telemetryView = new TextView(this);
        telemetryView.setTextColor(Color.GREEN);
        telemetryView.setTextSize(16);
        telemetryView.setTypeface(Typeface.MONOSPACE);
        telemetryView.setText("--");
        root.addView(telemetryView);

        actionButton = new Button(this);
        actionButton.setText("SCAN + CONNECT");
        actionButton.setOnClickListener(v -> onActionButton());
        root.addView(actionButton);

        logView = new TextView(this);
        logView.setTextColor(Color.LTGRAY);
        logView.setTextSize(12);
        logView.setTypeface(Typeface.MONOSPACE);
        logView.setMovementMethod(new ScrollingMovementMethod());
        logView.setVerticalScrollBarEnabled(true);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f);
        root.addView(logView, lp);

        setContentView(root);
    }

    private void log(String msg) {
        runOnUiThread(() -> {
            logView.append(msg + "\n");
            int scroll = logView.getLayout() == null ? 0
                    : logView.getLayout().getLineTop(logView.getLineCount())
                            - logView.getHeight();
            logView.scrollTo(0, Math.max(scroll, 0));
        });
    }

    private void setStatus(String s) {
        runOnUiThread(() -> statusView.setText(s));
    }

    private void onActionButton() {
        if (gatt != null || simRunning) {
            disconnect();
        } else if (ensurePermissions()) {
            startScan();
        }
    }

    // ---- permissions --------------------------------------------------------

    private boolean ensurePermissions() {
        List<String> needed = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 31) {
            for (String p : new String[]{Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT}) {
                if (checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) {
                    needed.add(p);
                }
            }
        } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        }
        if (needed.isEmpty()) return true;
        requestPermissions(needed.toArray(new String[0]), PERMISSION_REQ);
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int req, String[] perms, int[] res) {
        super.onRequestPermissionsResult(req, perms, res);
        if (req != PERMISSION_REQ) return;
        for (int r : res) {
            if (r != PackageManager.PERMISSION_GRANTED) {
                setStatus("Bluetooth permission denied");
                return;
            }
        }
        startScan();
    }

    // ---- scan + connect -----------------------------------------------------

    @SuppressWarnings("MissingPermission")
    private void startScan() {
        BluetoothManager bm = getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = bm.getAdapter();
        if (adapter == null || !adapter.isEnabled()) {
            setStatus("Bluetooth is off");
            return;
        }
        scanner = adapter.getBluetoothLeScanner();
        setStatus("Scanning for RC DIY...");
        actionButton.setText("STOP");

        ScanFilter filter = new ScanFilter.Builder()
                .setServiceUuid(new ParcelUuid(SERVICE_UUID)).build();
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();

        scanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int type, ScanResult result) {
                BluetoothDevice dev = result.getDevice();
                String name = result.getScanRecord() != null
                        ? result.getScanRecord().getDeviceName() : null;
                log("found " + name + " (" + dev.getAddress() + ") rssi "
                        + result.getRssi());
                stopScan();
                connect(dev, name == null ? dev.getAddress() : name);
            }

            @Override
            public void onScanFailed(int errorCode) {
                setStatus("Scan failed: " + errorCode);
            }
        };
        scanner.startScan(Collections.singletonList(filter), settings, scanCallback);
    }

    @SuppressWarnings("MissingPermission")
    private void stopScan() {
        if (scanner != null && scanCallback != null) {
            scanner.stopScan(scanCallback);
            scanCallback = null;
        }
    }

    @SuppressWarnings("MissingPermission")
    private void connect(BluetoothDevice device, String name) {
        setStatus("Connecting to " + name + "...");
        gatt = device.connectGatt(this, false, gattCallback,
                BluetoothDevice.TRANSPORT_LE);
    }

    @SuppressWarnings("MissingPermission")
    private void disconnect() {
        stopSim();
        stopScan();
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
            gatt = null;
        }
        configChar = null;
        notifyChar = null;
        equations.clear();
        pendingEquation.setLength(0);
        setStatus("RC Burner 0.2 — idle");
        runOnUiThread(() -> actionButton.setText("SCAN + CONNECT"));
    }

    // ---- GATT ---------------------------------------------------------------

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        @SuppressWarnings("MissingPermission")
        public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                log("connected, requesting MTU");
                g.requestMtu(185);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                log("disconnected (status " + status + ")");
                handler.post(MainActivity.this::disconnect);
            }
        }

        @Override
        @SuppressWarnings("MissingPermission")
        public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
            mtuPayload = mtu - 3;
            log("MTU " + mtu);
            g.discoverServices();
        }

        @Override
        @SuppressWarnings("MissingPermission")
        public void onServicesDiscovered(BluetoothGatt g, int status) {
            BluetoothGattService svc = g.getService(SERVICE_UUID);
            if (svc == null) {
                setStatus("Service 0x1ff8 not found");
                return;
            }
            configChar = svc.getCharacteristic(CONFIG_CHAR_UUID);
            notifyChar = svc.getCharacteristic(NOTIFY_CHAR_UUID);
            if (configChar == null || notifyChar == null) {
                setStatus("Characteristics missing");
                return;
            }
            // Subscribe to config indications; the firmware waits for this
            // before it starts registering its monitors.
            g.setCharacteristicNotification(configChar, true);
            BluetoothGattDescriptor cccd = configChar.getDescriptor(CCCD_UUID);
            cccd.setValue(BluetoothGattDescriptor.ENABLE_INDICATION_VALUE);
            g.writeDescriptor(cccd);
            setStatus("Connected, waiting for monitor config...");
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt g,
                BluetoothGattCharacteristic ch, byte[] value) {
            if (CONFIG_CHAR_UUID.equals(ch.getUuid())) {
                handleConfigCommand(value);
            }
        }

        // Pre-Android-13 callback signature
        @Override
        public void onCharacteristicChanged(BluetoothGatt g,
                BluetoothGattCharacteristic ch) {
            if (Build.VERSION.SDK_INT < 33 && CONFIG_CHAR_UUID.equals(ch.getUuid())) {
                handleConfigCommand(ch.getValue());
            }
        }
    };

    @SuppressWarnings("MissingPermission")
    private void handleConfigCommand(byte[] v) {
        if (v == null || v.length < 3) return;
        int cmdType = v[0] & 0xff;
        int monitorId = v[1] & 0xff;
        String payload = new String(v, 3, v.length - 3, StandardCharsets.UTF_8);

        switch (cmdType) {
            case CMD_TYPE_REMOVE_ALL:
                equations.clear();
                log("cfg: remove all");
                break;
            case CMD_TYPE_ADD_INCOMPLETE:
                pendingEquation.append(payload);
                return; // no ack for partial payloads
            case CMD_TYPE_ADD:
                pendingEquation.append(payload);
                String eq = pendingEquation.toString();
                pendingEquation.setLength(0);
                while (equations.size() <= monitorId) equations.add("");
                equations.set(monitorId, eq);
                log("cfg: monitor " + monitorId + " = " + eq);
                ackConfig(monitorId);
                startSimIfNeeded();
                break;
            case CMD_TYPE_REMOVE:
                if (monitorId < equations.size()) equations.set(monitorId, "");
                log("cfg: remove " + monitorId);
                break;
            default:
                log("cfg: unknown cmd " + cmdType);
        }
    }

    @SuppressWarnings("MissingPermission")
    private void ackConfig(int monitorId) {
        if (gatt == null || configChar == null) return;
        byte[] ack = {CMD_RESULT_OK, (byte) monitorId};
        if (Build.VERSION.SDK_INT >= 33) {
            gatt.writeCharacteristic(configChar, ack,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        } else {
            configChar.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
            configChar.setValue(ack);
            gatt.writeCharacteristic(configChar);
        }
    }

    // ---- simulation ---------------------------------------------------------

    private void startSimIfNeeded() {
        if (simRunning) return;
        simRunning = true;
        simStartMs = System.currentTimeMillis();
        lapStartMs = simStartMs;
        lapNumber = 1;
        prevLapDeci = 0x7fffffff;
        bestLapDeci = 0x7fffffff;
        deltaCs = 0;
        setStatus("Streaming simulated telemetry");
        handler.postDelayed(simTick, SIM_PERIOD_MS);
        log("sim: started");
    }

    private void stopSim() {
        simRunning = false;
        handler.removeCallbacks(simTick);
    }

    private final Runnable simTick = new Runnable() {
        @Override
        public void run() {
            if (!simRunning || gatt == null || notifyChar == null) return;
            long now = System.currentTimeMillis();

            // Lap rollover with slightly varying lap times
            if (now - lapStartMs >= lapDurationMs) {
                prevLapDeci = (int) (lapDurationMs / 100);
                if (prevLapDeci < bestLapDeci) bestLapDeci = prevLapDeci;
                lapNumber++;
                lapStartMs = now;
                lapDurationMs = 18000 + (long) (Math.random() * 4000);
                deltaCs = 0;
                log(String.format(Locale.US, "sim: lap %d, last %.1fs",
                        lapNumber, prevLapDeci / 10.0));
            }

            // Delta sim: smooth random walk clamped to ±2s, restarting at 0
            // on each lap crossing
            deltaCs += (int) Math.round((Math.random() - 0.5) * 16);
            if (deltaCs > 200) deltaCs = 200;
            if (deltaCs < -200) deltaCs = -200;

            // Speed: 40..250 km/h sweep, converted to m/s*10
            double phase = (now - simStartMs) / 20000.0 * 2 * Math.PI;
            double kmh = 145 + 105 * Math.sin(phase);
            int speedRaw = (int) Math.round(kmh / 3.6 * 10);
            int lapTimeDeci = (int) ((now - lapStartMs) / 100);

            ByteArrayOutputStream out = new ByteArrayOutputStream();
            synchronized (equations) {
                for (int id = 0; id < equations.size(); id++) {
                    String eq = equations.get(id);
                    if (eq.isEmpty()) continue;
                    int value;
                    // "delta_lap_time" must match before the generic "lap_time"
                    if (eq.contains("delta_lap_time")) {
                        // no comparison lap on the first lap -> invalid
                        value = lapNumber <= 1 ? 0x7fffffff : deltaCs;
                    }
                    else if (eq.contains("speed")) value = speedRaw;
                    else if (eq.contains("previous_lap_time")) value = prevLapDeci;
                    else if (eq.contains("best_lap_time")) value = bestLapDeci;
                    else if (eq.contains("lap_time")) value = lapTimeDeci;
                    else if (eq.contains("lap_number")) value = lapNumber;
                    else value = (int) ((now - simStartMs) / 1000);
                    out.write(id);
                    out.write((value >> 24) & 0xff);
                    out.write((value >> 16) & 0xff);
                    out.write((value >> 8) & 0xff);
                    out.write(value & 0xff);
                }
            }
            sendChunked(out.toByteArray());

            telemetryView.setText(String.format(Locale.US,
                    "%3.0f km/h  lap %d  %5.1fs  prev %s  best %s",
                    kmh, lapNumber, lapTimeDeci / 10.0,
                    fmtLap(prevLapDeci), fmtLap(bestLapDeci)));

            handler.postDelayed(this, SIM_PERIOD_MS);
        }
    };

    private static String fmtLap(int deci) {
        if (deci == 0x7fffffff) return "-:--.-";
        return String.format(Locale.US, "%d:%04.1f", deci / 600, (deci % 600) / 10.0);
    }

    @SuppressWarnings("MissingPermission")
    private void sendChunked(byte[] data) {
        // 5-byte tuples; keep each write inside the negotiated MTU payload
        int tuplesPerWrite = Math.max(1, mtuPayload / 5);
        for (int off = 0; off < data.length; off += tuplesPerWrite * 5) {
            int len = Math.min(tuplesPerWrite * 5, data.length - off);
            byte[] chunk = new byte[len];
            System.arraycopy(data, off, chunk, 0, len);
            if (Build.VERSION.SDK_INT >= 33) {
                gatt.writeCharacteristic(notifyChar, chunk,
                        BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
            } else {
                notifyChar.setWriteType(
                        BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
                notifyChar.setValue(chunk);
                gatt.writeCharacteristic(notifyChar);
            }
        }
    }

    @Override
    protected void onDestroy() {
        disconnect();
        super.onDestroy();
    }
}
