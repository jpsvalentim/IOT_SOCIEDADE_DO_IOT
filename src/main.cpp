#define MQTT_MAX_PACKET_SIZE 512


#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP_WiFiManager.h>
#include <PubSubClient.h>


const char* ssid = "FIESC_IOT";
const char* password = "C6qnM4ag81";
const char* mqtt_server = "test.mosquitto.org";


const int DHT11_PIN = 33;
const int LDR_PIN = 32;
const int SW520D_PIN = 4;
const int SW420_PIN = 13;
const int UMIDADESOLO_PIN = 12;

bool bmp_ok = false;
bool mpu_ok = false;


WiFiClient WOKWI_client;
PubSubClient client(WOKWI_client);
DHT dht(DHT11_PIN, DHT11);
Adafruit_MPU6050 mpu;
Adafruit_BMP085 bmp;


TwoWire MPU_I2C = TwoWire(0);
TwoWire BMP_I2C = TwoWire(1);


StaticJsonDocument<296> doc;


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("connecting to");
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


void setup_mpu6050() {
  MPU_I2C.begin(21, 22);
  mpu_ok = mpu.begin(0x68, &MPU_I2C);
  if (mpu_ok) {
    Serial.println("✅ MPU6050 inicializado com sucesso!");
  } else {
    Serial.println("❌ MPU6050 não encontrado!");
  }
}


void setup_bmp180() {
  BMP_I2C.begin(21, 22);
  bmp_ok = bmp.begin(BMP085_ULTRAHIGHRES, &BMP_I2C);
  if (bmp_ok) {
    Serial.println("BMP180 inicializado com sucesso!");
  } else {
    Serial.println("❌ BMP180 não encontrado!");
  }
}



void wifiReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT conection...");
    if (client.connect("WOKWI_client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rec=");
      Serial.print(client.state());
      Serial.println("Try again in 5 seconds");
     
      delay(5000);
    }
  }
}


void setup() {
  Serial.begin(115200);


  pinMode(18, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SW520D_PIN, INPUT);
  pinMode(SW420_PIN, INPUT);
  pinMode(UMIDADESOLO_PIN, INPUT);


  Serial.println("Iniciando...");


  setup_wifi();
  setup_mpu6050();
  setup_bmp180();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  Serial.println("🚀 ESP32 inicializado com sucesso!");
}


void Conectado_WiFi() {
  if (WiFi.status()) {
    digitalWrite(18, HIGH);
  } else {
    digitalWrite(18, LOW);
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
  int data = analogRead(LDR_PIN);
  float voltage = data * (3.3 / 4095.0);
  float rFixo = 10000;
  float resistance = (voltage * rFixo) / (3.3 - voltage);
  float lux = 500000.0 / resistance;


  doc["lux"] = lux;
}


void SW520D_value() {
  int inclinacao = digitalRead(SW520D_PIN);


  doc["inclinação"] = inclinacao;
}


void SW420_value() {
  int vibracao = digitalRead(SW420_PIN);


  doc["vibracao"] = vibracao;
}


void MPU6050_value() {
  sensors_event_t a, g, temp;

  if (mpu.getEvent(&a, &g, &temp)) {
    JsonObject acelerometro = doc.createNestedObject("acelerometro");
    acelerometro["x"] = a.acceleration.x;
    acelerometro["y"] = a.acceleration.y;
    acelerometro["z"] = a.acceleration.z;

    JsonObject giroscopio = doc.createNestedObject("giroscopio");
    giroscopio["x"] = g.gyro.x;
    giroscopio["y"] = g.gyro.y;
    giroscopio["z"] = g.gyro.z;
  } else {
    Serial.println("❌ Erro ao acessar MPU6050!");
  }
}

void DHT11_value() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();


  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;
}


void BMP180_value() {
  float pressao = bmp.readPressure();
  float altitude = bmp.readAltitude(101500);
  doc["pressao"] = pressao;
  doc["altitude"] = altitude;
}

void UMIDADESOLO_value() {
  float umidadeSolo = analogRead(UMIDADESOLO_PIN);


  doc["umidadeSolo"] = umidadeSolo;
}


void data_publish() {
  char buffer[256];
  serializeJson(doc, buffer);


  client.publish("monitoramento/encosta", buffer);


  Serial.println("JSON publicado:");
  serializeJsonPretty(doc, Serial);
  Serial.println();


  delay(2500);
}


void loop() {
  wifiReconnect();
  Conectado_WiFi();
  Conectado_broker();


  client.loop(); // <--- ESSENCIAL para MQTT funcionar corretamente

  doc.clear(); // <--- Limpa o JSON antes de preencher novamente

 

  LDR_value();
  SW520D_value();
  SW420_value();
 if (mpu_ok) {
  MPU6050_value();
}
  DHT11_value();
  if (bmp_ok) {
  BMP180_value();
}
  UMIDADESOLO_value();


  // data_publish();
}