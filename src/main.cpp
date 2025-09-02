#define MQTT_MAX_PACKET_SIZE 512

#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LED_STATUS_PIN 23 // led funcionamento do sistema (coleta de dados geral)
#define SDA_PIN 21       // i2c
#define SCL_PIN 22       // i2c
#define BUZZER_PIN 2     // buzzer

const char *ssid = "raquelis";
const char *password = "13082000";
const char *mqtt_server = "test.mosquitto.org";

const int DHT22_PIN = 33;
const int LDR_PIN = 32;
const int SW520D_PIN = 14;
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
  delay(10);
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi conectado!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi. Verifique SSID/senha.");
  }
}

void scanI2C() {
  Serial.println("Escaneando barramento I2C...");
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado no endereço 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println(" !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Erro desconhecido no endereço 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("Nenhum dispositivo I2C encontrado\n");
  } else {
    Serial.println("Escaneamento I2C concluído\n");
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
    Serial.println("WiFi desconectado. Tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(100);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Conectado!");
    } else {
      Serial.println("Falha na conexão WiFi.");
    }
  }
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("WOKWI_Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C(); // Diagnóstico I2C

  pinMode(5, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SW520D_PIN, INPUT);
  pinMode(SW420_PIN, INPUT);
  pinMode(UMIDADESOLO_PIN, INPUT);
  pinMode(GUVA, INPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Iniciando...");

  setup_wifi();
  setup_mpu6050();
  setup_bmp280();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  Serial.println("ESP32 inicializado com sucesso!");
}

void piscarStatusLED() {
  static unsigned long previousMillis = 0;
  const long interval = 500; // 0,5s

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Alterna LED
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));

    // Buzzer bip curto junto
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void Conectado_WiFi() {
  if (WiFi.status()) {
    digitalWrite(5, HIGH);
  } else {
    digitalWrite(5, LOW);
  }
}

void Conectado_broker() {
  if (client.connected()) {
    digitalWrite(19, HIGH);
  } else {
    digitalWrite(19, LOW);
  }
}

void LDR_value() {
  int lux = analogRead(LDR_PIN);
  doc["lux"] = lux;
  coletaAtiva = true;
}

void SW520D_value() {
  int inclinacao = digitalRead(SW520D_PIN);
  doc["inclinacao"] = inclinacao;
  coletaAtiva = true;
}

void SW420_value() {
  int vibracao = digitalRead(SW420_PIN);
  doc["vibracao"] = vibracao;
  coletaAtiva = true;
}

void MPU6050_value() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  JsonObject acelerometro = doc["acelerometro"].to<JsonObject>();
  acelerometro["x"] = a.acceleration.x;
  acelerometro["y"] = a.acceleration.y;
  acelerometro["z"] = a.acceleration.z;

  JsonObject giroscopio = doc["giroscopio"].to<JsonObject>();
  giroscopio["x"] = g.gyro.x;
  giroscopio["y"] = g.gyro.y;
  giroscopio["z"] = g.gyro.z;

  coletaAtiva = true;
}

void BMP280_value() {
  float pressao = bmp.readPressure();
  float altitude = bmp.readAltitude(101500);
  doc["pressao"] = pressao;
  doc["altitude"] = altitude;
  coletaAtiva = true;
}

void DHT22_value() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();
  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;
  coletaAtiva = true;
}

void UMIDADESOLO_value() {
  float umidadeSolo = analogRead(UMIDADESOLO_PIN);
  doc["umidadeSolo"] = umidadeSolo;
  coletaAtiva = true;
}

void RADIACAOUV_value() {
  float radiacaoUV = analogRead(GUVA);
  doc["radiacaoUV"] = radiacaoUV;
  coletaAtiva = true;
}

unsigned long lastMsg = 0;
unsigned long interval = 1000;

void data_publish() {
  char buffer[256];
  serializeJson(doc, buffer);

  unsigned long now = millis();

  if (now - lastMsg > interval) {
    lastMsg = now;

    client.publish("dispositivos/device1/dados", buffer);

    Serial.println("JSON publicado:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
}

void loop() {
  Conectado_broker();

  if (!client.connected()) {
    wifi_MQTT_Reconnect();
  }

  Conectado_WiFi();

  client.loop();

  doc.clear();

  LDR_value();
  SW520D_value();
  SW420_value();
  MPU6050_value();
  DHT22_value();
  BMP280_value();
  UMIDADESOLO_value();
  RADIACAOUV_value();

  data_publish();

  if (coletaAtiva) {
    piscarStatusLED();
    coletaAtiva = false;
  }
}

