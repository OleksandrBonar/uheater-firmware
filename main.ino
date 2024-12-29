#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLE2901.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <WiFi.h>

#ifndef BLE_SEC_ENABLE
//#define BLE_SEC_ENABLE 1
//#define BLE_SEC_NUMBER 123456
#endif

#define ON "ON"
#define OFF "OFF"

#define PIN_LED 2
#define PIN_SERVO 14
#define PIN_RELAY_A 25
#define PIN_RELAY_B 26
#define PIN_RELAY_C 27

#define APP_NAME "uHeater"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_MAIN_UUID "b0eb7b09-a92f-4cd7-a3ef-e009449bb46a"
#define SERVICE_WIFI_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVICE_MQTT_UUID "71842a85-784f-41d9-b4a3-d27994a35047"

// Main
#define CHARACTERISTIC_MAIN_BOOT_UUID "44d8a42d-9720-4adb-878d-5922094b247c"
#define CHARACTERISTIC_MAIN_MODE_UUID "85f369a6-4581-4d30-854f-65d4f9240cc6"

#define CHARACTERISTIC_MAIN_TMPA_UUID "9bf61fc0-f498-4a91-9077-f0997b9a25af"
#define CHARACTERISTIC_MAIN_TMPB_UUID "0655d6c3-6e50-4c84-ab94-6903e1176b72"

// WiFi
#define CHARACTERISTIC_WIFI_SSID_UUID "9c298009-647c-4a4d-86f7-6cd0bf2ceac0"
#define CHARACTERISTIC_WIFI_PASS_UUID "9d0a19d9-049b-445e-acf1-2c9c74dad82e"

// MQTT
#define CHARACTERISTIC_MQTT_HOST_UUID "47ecdd61-bb6f-47c7-abbc-d22a66c8bad4"
#define CHARACTERISTIC_MQTT_PORT_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_MQTT_USER_UUID "c89baae7-6efb-4149-8aed-1537d7b489b0"
#define CHARACTERISTIC_MQTT_PASS_UUID "116ce63e-c67f-470c-9380-4c32b4379c9d"

Preferences preferences;
WiFiClient wifi;
PubSubClient mqtt(wifi);
Servo servo;

int myCount = 0;
int maxCount = 4;

bool deviceConnected = false;
bool oldDeviceConnected = false;

int ledState = LOW;
long ledInterval = 500;
unsigned long ledCurrentMillis = 0;
unsigned long ledPreviousMillis = 0;

unsigned long wifiCurrentMillis = 0;
unsigned long wifiPreviousMillis = 0;
unsigned long wifiInterval = 20000;

unsigned long mqttCurrentMillis = 0;
unsigned long mqttPreviousMillis = 0;
unsigned long mqttInterval = 20000;

String main_mode = OFF;

String main_chna = OFF;
String main_chnb = OFF;
String main_chnc = OFF;

String main_tmpa = "0";
String main_tmpb = "0";

String wifi_ssid = "myssid";
String wifi_pass = "mypass";

String mqtt_host = "myhost";
String mqtt_port = "myport";
String mqtt_user = "myuser";
String mqtt_pass = "mypass";

void channel_handle(String name, String value, bool with_save = true) {
  if (with_save) {
    Serial.print("System: ");
    preferences_set("main", name, value);
  }

  const uint pin_num = name == "chna" ? PIN_RELAY_A : (name == "chnb" ? PIN_RELAY_B : PIN_RELAY_C);
  const uint pin_val = value == OFF ? HIGH : LOW;

  digitalWrite(pin_num, pin_val);
}

void servo_handle(String value) {
  Serial.print("System: ");

  const uint angle = map(value.toInt(), 38, 85, 0, 180);
  servo.write(angle);

  Serial.print("servo=");
  Serial.println(angle);
}

void mode_handle() {
  if (main_mode == OFF) {
    Serial.println("System: Heater disabling...");

    channel_handle("chna", OFF);
    delay(2000);

    Serial.println("System: Heater disabled");
  } else if (main_mode == "BOILER") {
    Serial.println("System: Boiler enabling...");

    channel_handle("chna", OFF);
    delay(2000);

    channel_handle("chnb", ON);
    delay(2000);

    servo_handle(main_tmpa);
    delay(2000);

    channel_handle("chna", ON);
    delay(2000);

    Serial.println("System: Boiler enabled");
  } else if (main_mode == "FLOOR") {
    Serial.println("System: Floor enabling...");

    channel_handle("chna", OFF);
    delay(2000);

    channel_handle("chnb", OFF);
    delay(2000);

    servo_handle(main_tmpb);
    delay(2000);

    channel_handle("chna", ON);
    delay(2000);

    Serial.println("System: Floor enabled");
  }
}

void preferences_set(String section, String param, String value) {
  Serial.print(section);
  Serial.print(".");
  Serial.print(param);
  Serial.print("=");
  Serial.println(value);

  preferences.begin(section.c_str());
  preferences.putString(param.c_str(), value);
  preferences.end();
}


class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Bluetooth: device connected");
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    Serial.println("Bluetooth: device disconnected");
    deviceConnected = false;

    pServer->startAdvertising();
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  String section = "";
  String param = "";

  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("Bluetooth: ");
      preferences_set(section, param, value);
    }
  }

public:
  CharacteristicCallbacks(String sectionName, String paramName) {
    section = sectionName;
    param = paramName;
  }
};

class CharacteristicMainBootCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      value.toUpperCase();
      if (value == "Y") {
        Serial.println("Bluetooth: rebooting...");
        delay(1000);
        ESP.restart();
      }
    }
  }
};

class CharacteristicMainModeCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("Bluetooth: ");
      preferences_set("main", "mode", value);

      main_mode = value;
      mode_handle();
    }
  }
};


void mqtt_callback(char* t, byte* p, unsigned int l) {
  String topic(t);
  String value;
  for (int i = 0; i < l; i++) {
    value.concat((char)p[i]);
  }

  if (topic == "Heater/mode/setValue") {
    Serial.print("MQTT: ");
    preferences_set("main", "mode", value);
    mqtt.publish("Heater/mode/getValue", value.c_str(), true);
  }

  if (topic == "Heater/temperatureA/setValue") {
    Serial.print("MQTT: ");
    preferences_set("main", "tmpa", value);
    mqtt.publish("Heater/temperatureA/getValue", value.c_str(), true);
  } else if (topic == "Heater/temperatureB/setValue") {
    Serial.print("MQTT: ");
    preferences_set("main", "tmpb", value);
    mqtt.publish("Heater/temperatureB/getValue", value.c_str(), true);
  }
}

void setupPreferences() {
  preferences.begin("main", false);
  main_mode = preferences.getString("mode", main_mode);
  main_chna = preferences.getString("chna", main_chna);
  main_chnb = preferences.getString("chnb", main_chnb);
  main_chnc = preferences.getString("chnc", main_chnc);
  main_tmpa = preferences.getString("tmpa", main_tmpa);
  main_tmpb = preferences.getString("tmpb", main_tmpb);
  preferences.end();

  preferences.begin("wifi", false);
  wifi_ssid = preferences.getString("ssid", wifi_ssid);
  wifi_pass = preferences.getString("pass", wifi_pass);
  preferences.end();

  preferences.begin("mqtt", false);
  mqtt_host = preferences.getString("host", mqtt_host);
  mqtt_port = preferences.getString("port", mqtt_port);
  mqtt_user = preferences.getString("user", mqtt_user);
  mqtt_pass = preferences.getString("pass", mqtt_pass);
  preferences.end();
}

void setupBluetooth() {
  BLEDevice::init(APP_NAME);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pServiceMain = pServer->createService(SERVICE_MAIN_UUID);

  BLECharacteristic *pCharacteristicMainBoot = pServiceMain->createCharacteristic(
    CHARACTERISTIC_MAIN_BOOT_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMainBoot->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMainBoot->setCallbacks(new CharacteristicMainBootCallbacks());

  BLECharacteristic *pCharacteristicMainMode = pServiceMain->createCharacteristic(
    CHARACTERISTIC_MAIN_MODE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMainMode->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMainMode->setCallbacks(new CharacteristicMainModeCallbacks());
  pCharacteristicMainMode->setValue(main_mode);

  BLECharacteristic *pCharacteristicMainTmpa = pServiceMain->createCharacteristic(
    CHARACTERISTIC_MAIN_TMPA_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMainTmpa->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMainTmpa->setCallbacks(new CharacteristicCallbacks("main", "tmpa"));
  pCharacteristicMainTmpa->setValue(main_tmpa);

  BLECharacteristic *pCharacteristicMainTmpb = pServiceMain->createCharacteristic(
    CHARACTERISTIC_MAIN_TMPB_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMainTmpb->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMainTmpb->setCallbacks(new CharacteristicCallbacks("main", "tmpb"));
  pCharacteristicMainTmpb->setValue(main_tmpb);

  pServiceMain->start();


  BLEService *pServiceWifi = pServer->createService(SERVICE_WIFI_UUID);

  BLECharacteristic *pCharacteristicWifiSsid = pServiceWifi->createCharacteristic(
    CHARACTERISTIC_WIFI_SSID_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicWifiSsid->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicWifiSsid->setCallbacks(new CharacteristicCallbacks("wifi", "ssid"));
  pCharacteristicWifiSsid->setValue(wifi_ssid);

  BLECharacteristic *pCharacteristicWifiPass = pServiceWifi->createCharacteristic(
    CHARACTERISTIC_WIFI_PASS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicWifiPass->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicWifiPass->setCallbacks(new CharacteristicCallbacks("wifi", "pass"));
  pCharacteristicWifiPass->setValue(wifi_pass);

  pServiceWifi->start();


  BLEService *pServiceMqtt = pServer->createService(SERVICE_MQTT_UUID);

  BLECharacteristic *pCharacteristicMqttHost = pServiceMqtt->createCharacteristic(
    CHARACTERISTIC_MQTT_HOST_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMqttHost->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMqttHost->setCallbacks(new CharacteristicCallbacks("mqtt", "host"));
  pCharacteristicMqttHost->setValue(mqtt_host);

  BLECharacteristic *pCharacteristicMqttPort = pServiceMqtt->createCharacteristic(
    CHARACTERISTIC_MQTT_PORT_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMqttPort->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMqttPort->setCallbacks(new CharacteristicCallbacks("mqtt", "port"));
  pCharacteristicMqttPort->setValue(mqtt_port);

  BLECharacteristic *pCharacteristicMqttUser = pServiceMqtt->createCharacteristic(
    CHARACTERISTIC_MQTT_USER_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMqttUser->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMqttUser->setCallbacks(new CharacteristicCallbacks("mqtt", "user"));
  pCharacteristicMqttUser->setValue(mqtt_user);

  BLECharacteristic *pCharacteristicMqttPass = pServiceMqtt->createCharacteristic(
    CHARACTERISTIC_MQTT_PASS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
#ifdef BLE_SEC_ENABLE
  pCharacteristicMqttPass->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pCharacteristicMqttPass->setCallbacks(new CharacteristicCallbacks("mqtt", "pass"));
  pCharacteristicMqttPass->setValue(mqtt_pass);

  pServiceMqtt->start();

#ifdef BLE_SEC_ENABLE
  //BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setStaticPIN(BLE_SEC_NUMBER);
#endif

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_MAIN_UUID);
  pAdvertising->addServiceUUID(SERVICE_WIFI_UUID);
  pAdvertising->addServiceUUID(SERVICE_MQTT_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);

  Serial.println();
  Serial.print("WiFi: connecting to ");
  Serial.print(wifi_ssid);

  WiFi.disconnect();
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  myCount = 0;
  while (WiFi.status() != WL_CONNECTED && myCount < maxCount) {
    myCount++;
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    randomSeed(micros());

    Serial.println();
    Serial.print("WiFi: connected ");
    Serial.print(wifi_ssid);
    Serial.print(" ");
    Serial.print(WiFi.localIP());
    Serial.print(" ");
    Serial.print(WiFi.macAddress());
    Serial.print(" ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println();
    Serial.print("WiFi: connection to ");
    Serial.print(wifi_ssid);
    Serial.println(" failed, try again in 2 seconds");
  }
}

void setupMqtt() {
  mqtt.setServer(mqtt_host.c_str(), mqtt_port.toInt());
  mqtt.setCallback(mqtt_callback);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT);
  pinMode(PIN_RELAY_A, OUTPUT);
  pinMode(PIN_RELAY_B, OUTPUT);
  pinMode(PIN_RELAY_C, OUTPUT);

  servo.attach(PIN_SERVO);

  setupPreferences();

  channel_handle("chna", main_chna, false);
  channel_handle("chnb", main_chnb, false);
  channel_handle("chnc", main_chnc, false);

  mode_handle();

  setupBluetooth();
  setupWifi();
  setupMqtt();
}

void loop() {
  if (!WiFi.isConnected()) {
    ledInterval = 500;
  } else if (!mqtt.connected()) {
    ledInterval = 1000;
  } else {
    ledInterval = 0;
  }

  if (ledInterval > 0) {
    ledCurrentMillis = millis();
    if (ledCurrentMillis - ledPreviousMillis >= ledInterval) {
      ledPreviousMillis = ledCurrentMillis;

      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }

      digitalWrite(PIN_LED, ledState);
    }
  } else {
    digitalWrite(PIN_LED, HIGH);
  }

  // if WiFi is down, try reconnecting every 2 seconds
  wifiCurrentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (wifiCurrentMillis - wifiPreviousMillis >= wifiInterval)) {
    Serial.print("WiFi: reconnecting to ");
    Serial.print(wifi_ssid);
    Serial.println("...");
    WiFi.disconnect();
    WiFi.reconnect();
    wifiPreviousMillis = wifiCurrentMillis;
  }

  // if MQTT is down, try reconnecting every 2 seconds
  mqttCurrentMillis = millis();
  if (WiFi.isConnected()) {
    if (!mqtt.connected() && (mqttCurrentMillis - mqttPreviousMillis >= mqttInterval)) {
      mqtt.disconnect();
      Serial.print("MQTT: connecting to ");
      Serial.print(mqtt_host);
      Serial.print(":");
      Serial.print(mqtt_port);
      Serial.print(" ");
      Serial.print(mqtt_user);
      Serial.println("...");
      if (mqtt.connect(APP_NAME, mqtt_user.c_str(), mqtt_pass.c_str())) {
        Serial.print("MQTT: connected ");
        Serial.print(mqtt_host);
        Serial.print(":");
        Serial.print(mqtt_port);
        Serial.print(" ");
        Serial.println(mqtt_user);
        // Once connected, publish an announcement...
        mqtt.publish("Heater/mode/getValue", main_mode.c_str(), true);
        mqtt.publish("Heater/temperatureA/getValue", main_tmpa.c_str(), true);
        mqtt.publish("Heater/temperatureB/getValue", main_tmpb.c_str(), true);
        // ... and resubscribe
        mqtt.subscribe("Heater/mode/setValue");
        mqtt.subscribe("Heater/temperatureA/setValue");
        mqtt.subscribe("Heater/temperatureB/setValue");
      } else {
        Serial.print("MQTT: connection ");
        Serial.print(mqtt_host);
        Serial.print(":");
        Serial.print(mqtt_port);
        Serial.print(" ");
        Serial.print(mqtt_user);
        Serial.print(" failed, rc=");
        Serial.print(mqtt.state());
        Serial.println(" try again in 2 seconds");
      }

      mqttPreviousMillis = mqttCurrentMillis;
    }

    mqtt.loop();
  }

  // // notify changed value
  // if (deviceConnected) {
  //   pCharacteristic->setValue((uint8_t *)&value, 4);
  //   pCharacteristic->notify();
  //   value++;
  //   delay(500);
  // }
  // // disconnecting
  // if (!deviceConnected && oldDeviceConnected) {
  //   delay(500);                   // give the bluetooth stack the chance to get things ready
  //   pServer->startAdvertising();  // restart advertising
  //   Serial.println("start advertising");
  //   oldDeviceConnected = deviceConnected;
  // }
  // // connecting
  // if (deviceConnected && !oldDeviceConnected) {
  //   // do stuff here on connecting
  //   oldDeviceConnected = deviceConnected;
  // }
}
