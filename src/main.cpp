#define MQTT_MAX_PACKET_SIZE 512

#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <PubSubClient.h>

#define LED_STATUS_PIN 23
#define SDA_PIN 21
#define SCL_PIN 22
#define BUZZER_PIN 2

const char *ssid = "raquelis";
const char *password = "13082000";
const char *mqtt_server = "test.mosquitto.org";

const int DHT22_PIN = 33;
const int LDR_PIN = 32;
const int SW520D_PIN 14;
const int SW420_PIN = 13;
const int GUVA = 35;
const int UMIDADESOLO_PIN = 34;

bool bmp_ok = false;
bool mpu_ok = false;
bool coletaAtiva = false;

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT22_PIN, DHT22);
Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;
StaticJsonDocument<512> doc;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado! IP: " + WiFi.localIP().toString());
  }
}

void setup_mpu6050() {
  if (!mpu.begin()) {
    Serial.println("MPU6050 não encontrado!");
    mpu_ok = false;
    return;
  }
  mpu_ok = true;
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  Serial.println("MPU6050 OK");
}

void setup_bmp280() {
  if (!bmp.begin(0x76)) {
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 não encontrado em 0x76 ou 0x77!");
      bmp_ok = false;
      return;
    }
  }
  bmp_ok = true;
  Serial.println("BMP280 OK");
}

void wifi_MQTT_Reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(5000); // Temporário, substitua por millis()
  }
  while (!client.connected()) {
    if (client.connect("ESP32_Client")) {
      Serial.println("MQTT conectado");
    } else {
      Serial.println("MQTT falhou, rc=" + String(client.state()));
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C(); // Diagnóstico I2C

  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(5, OUTPUT); // WiFi
  pinMode(19, OUTPUT); // MQTT

  setup_wifi();
  setup_mpu6050();
  setup_bmp280();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  Serial.println("ESP32 inicializado!");
}

void piscarStatusLED() {
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) {
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
    lastBlink = millis();
  }
}

void MPU6050_value() {
  if (!mpu_ok) return;
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    doc["acelerometro"]["x"] = a.acceleration.x;
    doc["acelerometro"]["y"] = a.acceleration.y;
    doc["acelerometro"]["z"] = a.acceleration.z;
    doc["giroscopio"]["x"] = g.gyro.x;
    doc["giroscopio"]["y"] = g.gyro.y;
    doc["giroscopio"]["z"] = g.gyro.z;
    coletaAtiva = true;
  }
}

void BMP280_value() {
  if (!bmp_ok) return;
  doc["pressao"] = bmp.readPressure();
  doc["altitude"] = bmp.readAltitude(101500);
  coletaAtiva = true;
}

// [Adicione outras funções como LDR_value, etc., com millis()]
void loop() {
  client.loop();
  if (!client.connected()) wifi_MQTT_Reconnect();

  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 1000) {
    doc.clear();
    coletaAtiva = false;
    MPU6050_value();
    BMP280_value(); // Teste só esses primeiro
    if (coletaAtiva) piscarStatusLED();
    lastRead = millis();
  }
}

void scanI2C() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Scanning I2C...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Device at 0x");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) Serial.println("No I2C devices!");
}