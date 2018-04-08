package sharrington.reflowovencontroller;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.*;
import android.content.Intent;

import java.io.IOException;
import java.util.Set;

import android.graphics.Color;
import android.os.Message;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.os.Handler;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewDebug;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.jjoe64.graphview.GraphView;

import com.jjoe64.graphview.series.DataPoint;
import com.jjoe64.graphview.series.LineGraphSeries;


public class Reflow extends AppCompatActivity {

    // Layout view
    private TextView mTitle;
    // Name of the connected device
    private String mConnectedDeviceName = null;

    private BluetoothAdapter mBluetoothAdapter = null;
    private BluetoothChatService mCommandService = null;
    private BluetoothSocket mBluetoothSocket = null;
    // Intent request codes
    // Intent request codes
    private static final int REQUEST_CONNECT_DEVICE_SECURE = 1;
    private static final int REQUEST_CONNECT_DEVICE_INSECURE = 2;
    private static final int REQUEST_ENABLE_BT = 3;

    private GraphView graph;
    private LineGraphSeries<DataPoint> tempSeries;
    private LineGraphSeries<DataPoint> desiredTempSeries;
    private double lastTime = 0;

    private boolean reflowStart = false;

    // Message types
    final private int HEARTBEAT = 0;
    final private int REPLY = 1;
    final private int COMMAND = 2;
    final private int DEBUG = 3;

    // States
    final private int INIT = 0;
    final private int STANDBY = 1;
    final private int CONFIG = 2;
    final private int PREHEAT = 3;
    final private int REFLOW = 4;
    final private int ERROR = 5;

    // Controller state
    private int controllerState = INIT;

    // Controller errors
    private int controllerErrors = 0;

    // Debug mode
    private boolean debugModeEnabled = false;
    double desiredTemp = 0;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_reflow);

        mTitle = (TextView) findViewById(R.id.title_left_text);
        mTitle.setText(R.string.app_name);
        mTitle = (TextView) findViewById(R.id.title_right_text);

        graph = (GraphView) findViewById(R.id.graph);

        graph.getViewport().setScrollable(true);
        graph.getViewport().setYAxisBoundsManual(true);
        graph.getViewport().setXAxisBoundsManual(true);
        graph.getViewport().setMaxY(300);
        graph.getViewport().setMinY(0);
        graph.getViewport().setMaxX(250);
        graph.getViewport().setMinX(0);


        tempSeries = new LineGraphSeries<>();
        tempSeries.setDrawBackground(true);
        tempSeries.setColor(Color.RED);
        tempSeries.setBackgroundColor(Color.argb(150, 255, 0, 0));
        graph.addSeries(tempSeries);

        desiredTempSeries = new LineGraphSeries<>();
        desiredTempSeries.setColor(Color.GREEN);
        graph.addSeries(desiredTempSeries);

        controllerState = STANDBY;
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        // If the adapter is null, then Bluetooth is not supported
        if (mBluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth is not available", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        else
        {
            if (!mBluetoothAdapter.isEnabled()) {
                Intent enableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                startActivityForResult(enableIntent, REQUEST_ENABLE_BT);
            }
            // otherwise set up the command service
            else {
                if (mCommandService==null)
                    setupCommand();
            }
        }
    }


    @Override
    protected void onResume() {
        super.onResume();

        // Performing this check in onResume() covers the case in which BT was
        // not enabled during onStart(), so we were paused to enable it...
        // onResume() will be called when ACTION_REQUEST_ENABLE activity returns.
        if (mCommandService != null) {
            if (mCommandService.getState() == BluetoothChatService.STATE_NONE) {
                mCommandService.start();
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mCommandService != null)
            mCommandService.stop();
    }

    private void ensureDiscoverable() {
        if (mBluetoothAdapter.getScanMode() !=
                BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
            Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
            startActivity(discoverableIntent);
        }
    }

    public void bluetoothConnect(View view)
    {

    }

    private void setupCommand() {
        // Initialize the BluetoothChatService to perform bluetooth connections
        mCommandService = new BluetoothChatService(this, mHandler);
    }

    // The Handler that gets information back from the BluetoothChatService
    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case Constants.MESSAGE_STATE_CHANGE:
                    switch (msg.arg1) {
                        case BluetoothChatService.STATE_CONNECTED:
                            mTitle.setText(R.string.title_connected_to);
                            mTitle.append(mConnectedDeviceName);
                            break;
                        case BluetoothChatService.STATE_CONNECTING:
                            mTitle.setText(R.string.title_connecting);
                            break;
                        case BluetoothChatService.STATE_LISTEN:
                        case BluetoothChatService.STATE_NONE:
                            mTitle.setText(R.string.title_not_connected);
                            break;
                    }
                    break;
                case Constants.MESSAGE_DEVICE_NAME:
                    // save the connected device's name
                    mConnectedDeviceName = msg.getData().getString(Constants.DEVICE_NAME);
                    Toast.makeText(getApplicationContext(), "Connected to "
                            + mConnectedDeviceName, Toast.LENGTH_SHORT).show();
                    break;
                case Constants.MESSAGE_TOAST:
                    Toast.makeText(getApplicationContext(), msg.getData().getString(Constants.TOAST),
                            Toast.LENGTH_SHORT).show();
                    break;
                case Constants.MESSAGE_READ:
                    byte[] readBuf = (byte[]) msg.obj;
                    float temperature = 0;

                    switch (parseMessageType(readBuf[0]))
                    {
                        case HEARTBEAT:
                            // Read any MCU errors if any
                            parseControllerError(readBuf);

                            if (controllerErrors == 0)
                            {
                                // Parse MCU state out of heartbeat
                                parseControllerState(readBuf[0]);
                                switch (controllerState) {
                                    // Init ... not possible to receive this state at the moment
                                    case INIT:
                                        break;

                                    // MCU is waiting in standby for a command
                                    case STANDBY:
                                        temperature = parseTemperatureC(readBuf);
                                        tempSeries.appendData(new DataPoint(lastTime, temperature), true, 500);
                                        reflowStart = true;
                                        break;

                                    // MCU is in configuration mode
                                    case CONFIG:
                                        break;

                                    // Reflow oven is preheating
                                    case PREHEAT:
                                        temperature = parseTemperatureC(readBuf);
                                        tempSeries.appendData(new DataPoint(lastTime, temperature), true, 500);
                                        break;

                                    // In the reflow process
                                    case REFLOW:
                                        temperature = parseTemperatureC(readBuf);
                                        tempSeries.appendData(new DataPoint(lastTime, temperature), true, 500);

                                        if (debugModeEnabled == true) {
                                            desiredTempSeries.appendData(new DataPoint(lastTime, desiredTemp), true, 500);
                                        }
                                        // Add desired reflow temperature data series
                                        if (reflowStart == true) {
                                            //addDesiredTemperatureSeries();
                                            reflowStart = false;
                                        }
                                        break;

                                    // MCU is in an error state
                                    case ERROR:
                                        break;

                                    // Something is wrong. Try to command MCU to soft fault
                                    default:
                                        break;
                                }
                            }
                            else
                            {
                                // Display errors. Allow user to clear errors
                            }
                            lastTime = lastTime + 0.5;
                            break;

                        case REPLY:

                            break;

                        case COMMAND:

                            break;

                        case DEBUG:
                            debugModeEnabled = true;
                            desiredTemp = parseDebugMessage(readBuf);
                            return;

                        default:

                            return;
                    }







                    //Toast.makeText(getApplicationContext(), readMessage, Toast.LENGTH_SHORT).show();

                    break;
            }
        }
    };

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_CONNECT_DEVICE_SECURE:
                // When DeviceListActivity returns with a device to connect
                if (resultCode == Activity.RESULT_OK) {
                    connectDevice(data, true);
                }
                break;
            case REQUEST_CONNECT_DEVICE_INSECURE:
                // When DeviceListActivity returns with a device to connect
                if (resultCode == Activity.RESULT_OK) {
                    connectDevice(data, false);
                }
                break;
            case REQUEST_ENABLE_BT:
                // When the request to enable Bluetooth returns
                if (resultCode == Activity.RESULT_OK) {
                    // Bluetooth is now enabled, so set up a chat session
                    setupChat();
                } else {
                    // User did not enable Bluetooth or an error occurred
                    Toast.makeText(this, R.string.bt_not_enabled_leaving,
                            Toast.LENGTH_SHORT).show();
                    finish();
                }
        }
    }

    /**
     * Establish connection with other device
     *
     * @param data   An {@link Intent} with {@link DeviceListActivity#EXTRA_DEVICE_ADDRESS} extra.
     * @param secure Socket Security type - Secure (true) , Insecure (false)
     */
    private void connectDevice(Intent data, boolean secure) {
        // Get the device MAC address
        String address = data.getExtras()
                .getString(DeviceListActivity.EXTRA_DEVICE_ADDRESS);
        // Get the BluetoothDevice object
        BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
        // Attempt to connect to the device
        mCommandService.connect(device, secure);
    }

    /**
     * Set up the UI and background operations for chat.
     */
    private void setupChat() {


        // Initialize the BluetoothChatService to perform bluetooth connections
        mCommandService = new BluetoothChatService(this, mHandler);

    }

    private void parseControllerError(byte[] buffer)
    {

    }

    private void parseControllerState(byte infoByte)
    {
        int messageType = (infoByte & 0xC0) >> 6;
        // Not heartbeat, can't determine state
        if (messageType != 0)
        {
            return;
        }

        controllerState = infoByte & 0xF;
    }

    private int parseMessageType(byte infoByte)
    {
        return (infoByte & 0xC0) >> 6;
    }

    private double parseDebugMessage(byte[] buffer)
    {
        int firstByte = (int) buffer[2] & 0xFF;
        int secondByte = (int) buffer[1] & 0xFF;
        double desiredTemp = (double)((secondByte << 8) + (firstByte));
        return desiredTemp;
    }

    private float parseTemperatureC(byte[] buffer)
    {
        int temperatureData = ((buffer[1] << 8) + (buffer[2] & 0xFC)) >> 2;
        double remainder = 0.25 * (temperatureData & (0x3));
        float temperature = (float)(temperatureData >> 2) + (float)(remainder);
        return temperature;
    }

    private void addDesiredTemperatureSeries()
    {
        LineGraphSeries<DataPoint> series = new LineGraphSeries<>(new DataPoint[] {
                new DataPoint(lastTime + 0,21.6049382716049),
                new DataPoint(lastTime + 2.10526315789473,26.6298462204894),
                new DataPoint(lastTime + 4.56140350877193,33.8296873871921),
                new DataPoint(lastTime + 6.66666666666667,40.39780521262),
                new DataPoint(lastTime + 9.12280701754386,47.2890044040141),
                new DataPoint(lastTime + 11.2280701754385,54.1657642047505),
                new DataPoint(lastTime + 12.9824561403508,59.7935167135946),
                new DataPoint(lastTime + 15.0877192982456,66.0529925637137),
                new DataPoint(lastTime + 17.1929824561403,72.9297523644502),
                new DataPoint(lastTime + 19.6491228070175,78.5863836546097),
                new DataPoint(lastTime + 21.7543859649122,84.8458595047288),
                new DataPoint(lastTime + 23.859649122807,90.4880514042307),
                new DataPoint(lastTime + 26.3157894736842,97.3792505956248),
                new DataPoint(lastTime + 28.7719298245614,103.653165836401),
                new DataPoint(lastTime + 30.8771929824561,109.295357735903),
                new DataPoint(lastTime + 33.6842105263157,114.96642841672),
                new DataPoint(lastTime + 36.1403508771929,120.62305970688),
                new DataPoint(lastTime + 38.9473684210526,127.528698288932),
                new DataPoint(lastTime + 41.7543859649122,131.965201068514),
                new DataPoint(lastTime + 44.5614035087719,137.018987798714),
                new DataPoint(lastTime + 47.719298245614,141.469929968955),
                new DataPoint(lastTime + 50.8771929824561,144.686304237961),
                new DataPoint(lastTime + 54.0350877192982,148.519962457584),
                new DataPoint(lastTime + 57.1929824561403,151.119052775972),
                new DataPoint(lastTime + 60,153.086419753086),
                new DataPoint(lastTime + 63.5087719298245,155.699949462132),
                new DataPoint(lastTime + 67.0175438596491,157.696195220561),
                new DataPoint(lastTime + 70.1754385964912,159.678001588333),
                new DataPoint(lastTime + 73.6842105263158,162.291531297379),
                new DataPoint(lastTime + 77.8947368421052,164.316655837123),
                new DataPoint(lastTime + 82.1052631578947,166.959064327485),
                new DataPoint(lastTime + 85.6140350877193,169.572594036531),
                new DataPoint(lastTime + 89.4736842105263,171.583279185618),
                new DataPoint(lastTime + 92.9824561403509,174.196808894664),
                new DataPoint(lastTime + 96.140350877193,177.41318316367),
                new DataPoint(lastTime + 99.6491228070175,180.026712872716),
                new DataPoint(lastTime + 103.508771929824,183.271965923038),
                new DataPoint(lastTime + 107.017543859649,187.737347483936),
                new DataPoint(lastTime + 110.175438596491,191.571005703559),
                new DataPoint(lastTime + 113.333333333333,195.404663923182),
                new DataPoint(lastTime + 116.842105263157,199.252761533463),
                new DataPoint(lastTime + 120,204.320987654321),
                new DataPoint(lastTime + 123.157894736842,208.771929824561),
                new DataPoint(lastTime + 125.614035087719,212.576709262869),
                new DataPoint(lastTime + 128.070175438596,216.998772651794),
                new DataPoint(lastTime + 131.228070175438,220.832430871417),
                new DataPoint(lastTime + 133.684210526315,225.254494260342),
                new DataPoint(lastTime + 137.543859649122,230.351599162515),
                new DataPoint(lastTime + 139.298245614035,234.744783770124),
                new DataPoint(lastTime + 142.456140350877,239.195725940365),
                new DataPoint(lastTime + 145.614035087719,242.412100209371),
                new DataPoint(lastTime + 148.771929824561,246.245758428994),
                new DataPoint(lastTime + 151.578947368421,249.447693307342),
                new DataPoint(lastTime + 154.736842105263,252.664067576348),
                new DataPoint(lastTime + 158.59649122807,255.909320626669),
                new DataPoint(lastTime + 161.754385964912,257.89112699444),
                new DataPoint(lastTime + 164.561403508771,259.858493971554),
                new DataPoint(lastTime + 167.719298245614,261.223016388708),
                new DataPoint(lastTime + 170.526315789473,261.955815464587),
                new DataPoint(lastTime + 173.333333333333,263.305898491083),
                new DataPoint(lastTime + 176.140350877193,263.421413616345),
                new DataPoint(lastTime + 178.947368421052,262.919644790989),
                new DataPoint(lastTime + 181.754385964912,262.417875965634),
                new DataPoint(lastTime + 184.561403508772,261.916107140278),
                new DataPoint(lastTime + 187.017543859649,260.16533102303),
                new DataPoint(lastTime + 189.824561403508,257.811710345823),
                new DataPoint(lastTime + 191.929824561403,254.811926936683),
                new DataPoint(lastTime + 194.38596491228,251.209298967583),
                new DataPoint(lastTime + 196.491228070175,246.357663706591),
                new DataPoint(lastTime + 198.59649122807,240.888744494982),
                new DataPoint(lastTime + 200.350877192982,236.022669843332),
                new DataPoint(lastTime + 201.754385964912,231.142155801025),
                new DataPoint(lastTime + 202.807017543859,226.24720236806),
                new DataPoint(lastTime + 204.561403508772,221.38112771641),
                new DataPoint(lastTime + 205.964912280701,217.11789762472),
                new DataPoint(lastTime + 207.368421052631,211.620099631795),
                new DataPoint(lastTime + 208.421052631579,206.72514619883),
                new DataPoint(lastTime + 209.824561403508,201.227348205905),
                new DataPoint(lastTime + 210.877192982456,196.33239477294),
                new DataPoint(lastTime + 212.631578947368,192.700888022525),
                new DataPoint(lastTime + 214.035087719298,187.2030900296),
                new DataPoint(lastTime + 215.087719298245,182.308136596635),
                new DataPoint(lastTime + 216.140350877193,177.41318316367),
                new DataPoint(lastTime + 217.894736842105,171.312540610786),
                new DataPoint(lastTime + 218.947368421052,166.417587177821),
                new DataPoint(lastTime + 220.350877192982,160.919789184896),
                new DataPoint(lastTime + 221.754385964912,155.421991191971),
                new DataPoint(lastTime + 223.157894736842,150.541477149664),
                new DataPoint(lastTime + 224.210526315789,144.411955815464),
                new DataPoint(lastTime + 225.614035087719,138.296873871922),
                new DataPoint(lastTime + 227.017543859649,132.799075878997),
                new DataPoint(lastTime + 228.421052631579,126.683993935455),
                new DataPoint(lastTime + 230.526315789473,119.980506822612),
                new DataPoint(lastTime + 231.578947368421,115.085553389647),
                new DataPoint(lastTime + 233.333333333333,111.454046639231),
                new DataPoint(lastTime + 233.684210526315,106.530214424951),
                new DataPoint(lastTime + 235.78947368421,99.8267273121074),
                new DataPoint(lastTime + 236.842105263157,93.0799220272905),
                new DataPoint(lastTime + 238.947368421052,87.6110028156812),
                new DataPoint(lastTime + 239.986504723346,82.3593686654115),
                new DataPoint(lastTime + 241.403508771929,76.7434175816242),
                new DataPoint(lastTime + 242.820512820512,71.1274664978369),
                new DataPoint(lastTime + 244.237516869095,65.5115154140497),
                new DataPoint(lastTime + 245.654520917678,59.8955643302624),
                new DataPoint(lastTime + 247.071524966261,54.2796132464752),
                new DataPoint(lastTime + 248.488529014844,48.6636621626882),
                new DataPoint(lastTime + 249.905533063427,43.0477110789002),
                new DataPoint(lastTime + 251.32253711201,37.4317599951132),
                new DataPoint(lastTime + 252.739541160593,31.8158089113262),
                new DataPoint(lastTime + 254.156545209176,26.1998578275382),
                new DataPoint(lastTime + 255.573549257759,20.5839067437512)
        });
        graph.addSeries(series);
        series.setDrawBackground(true);

    }

    public void onStartReflowButtonPress(View view) {
        Button buttonSrc = (Button) findViewById(R.id.button);
        if (buttonSrc.getText().toString().equals("Start Reflow")) {
            byte[] command = {(byte) 0x8C, (byte) 0x00, (byte) 0x00};
            mCommandService.write(command);

            buttonSrc.setText("Stop Reflow");
        }
        else
        {
            byte[] command = {(byte) 0x8C, (byte) 0x00, (byte) 0xFF};
            mCommandService.write(command);

            buttonSrc.setText("Start Reflow");
        }
    }

    public void onConfigurePIDButtonPress(View view) {
        startActivity(new Intent(Reflow.this, PopupPID.class));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.option_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.scan:
                // Launch the DeviceListActivity to see devices and do scan
                Intent serverIntent = new Intent(this, DeviceListActivity.class);
                startActivityForResult(serverIntent, REQUEST_CONNECT_DEVICE_SECURE);
                return true;
            case R.id.discoverable:
                // Ensure this device is discoverable by others
                ensureDiscoverable();
                return true;
        }
        return false;
    }
}
