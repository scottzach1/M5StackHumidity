#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <M5Stack.h>

/**
 * BLE Related stuff
 * 
 * <https://btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf>
 */
static BLEUUID serviceUUID = BLEUUID("4bf524fc-e77c-4b80-bbc6-1345b5f41d76");
static BLEUUID humiCharacteristicUUID = ((uint16_t)0x2A6F);

BLEDescriptor humiDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic humiCharacteristic(humiCharacteristicUUID, BLECharacteristic::PROPERTY_READ);

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLEAdvertising *pAdvertising = NULL;

bool deviceConnected = false;

/**
 * Duty Cycling Timeouts
 */
const int DUTY_CYCLE_AWAKE = 4;  // seconds awake
const int DUTY_CYCLE_SLEEP = 4;  // sedons asleep

/**
 * Safe memory (persistent through deepSleeps).
 */
RTC_DATA_ATTR time_t timestamp = 0;
RTC_DATA_ATTR time_t activityTimestamp = 0;
RTC_DATA_ATTR bool dutyCycle = false;

// Safe previous reading buffers for persistent readings.
RTC_DATA_ATTR uint8_t curHumi = 0;

/**
 * Callbacks for when we connect/disconnect from client.
 */
class MyServerCallbacks : public BLEServerCallbacks {
    /**
     * Update device connected state upon connection.
     */
    void onConnect(BLEServer *pServer) {
        time(&activityTimestamp);
        M5.Lcd.println("client connected");
        deviceConnected = true;
    };

    /**
     * Update device connected state upon disconnection.
     * 
     * Unfortunately, due to problems I am having with the M5Stack, the sensor node refuses to advertise
     * once it has connected and disconnected from a client. By deep sleeping we update the sensor state
     * and reset the BLE device.
     */
    void onDisconnect(BLEServer *pServer) {
        M5.Lcd.println("client disconnected");
        deviceConnected = false;
        M5.Power.deepSleep(SLEEP_MSEC(10));  // ↑ See descripiton! ↑
        delayMicroseconds(10);
    }
};

/**
 * Generate a random humidity within boundaries, then update and return humidity buffer address.
 */
uint8_t *updateRandHumi() {
    time(&activityTimestamp);
    curHumi = (uint8_t)(rand() % 100);
    M5.Lcd.println(curHumi);
    return &curHumi;
}

/**
 * Callback invoked when the Humidity characteristic is read.
 */
class HumiCallBacks : public BLECharacteristicCallbacks {
    /**
     * Generate random humidity and respond to client.
     */
    void onRead(BLECharacteristic *pCharacteristic) {
        pCharacteristic->setValue((uint8_t *)updateRandHumi(), 2);
    }
};

/**
 * Configures the critical sensor node peripherals such as screen and BLE server.
 */
void setup() {
    // Initialize device
    Serial.begin(115200);
    M5.begin();
    M5.Power.begin();
    M5.Lcd.clear(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setBrightness(75);
    M5.Lcd.println("Humidity node starting...");

    // Create BLE server with callbacks.
    BLEDevice::init("m5-humidity-1");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    pService = pServer->createService(serviceUUID);

    // Add descriptor values.
    humiDescriptor.setValue("Humitidy: [0,100]%");

    // Update characteristics with descriptors.
    humiCharacteristic.addDescriptor(&humiDescriptor);

    // Add callback handlers to characteristics.
    humiCharacteristic.setCallbacks(new HumiCallBacks());

    // Display advertised UUIDs for debbugging.
    M5.Lcd.printf("- Serv-UUID: %s\n", serviceUUID.toString().c_str());
    M5.Lcd.printf("- Humi-UUID: %s\n", humiCharacteristic.getUUID().toString().c_str());

    // Add characteristics to service.
    pService->addCharacteristic(&humiCharacteristic);

    // Start service and begin advertising.
    pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(serviceUUID);
    pService->start();
    pAdvertising->start();

    time(&activityTimestamp);
}

/**
 * Clears the display and resets cursor position.
 */
void clearDisplay() {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setCursor(0, 0);
}

/**
 * Toggles duty cycle, notifying Lcd and updating activity timeout.
 */
void toggleDutyCycle() {
    dutyCycle = !dutyCycle;
    M5.Lcd.println("SET DUTY_CYCLE " + String(dutyCycle));
    time(&activityTimestamp);
}

/**
 * Main event loop, listens for buttons and checks duty cycle based on timestamps.
 */
void loop() {
    M5.update();

    // Handle button presses.
    if (M5.BtnB.wasReleasefor(5)) toggleDutyCycle();
    if (M5.BtnC.wasReleasefor(5)) M5.Power.reset();

    time(&timestamp);
    // Trigger duty cycle sleep only after threshold.
    if (dutyCycle && timestamp - activityTimestamp > DUTY_CYCLE_AWAKE) {
        M5.Power.deepSleep(SLEEP_SEC(DUTY_CYCLE_SLEEP));
    }
}
