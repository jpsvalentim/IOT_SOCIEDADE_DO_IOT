#define MQTT_MAX_PACKET_SIZE 512

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME680.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP_WiFiManager.h>

const char *ssid = "Juia";
const char *password = "12345678";
const char *mqtt_server = "test.mosquitto.org";

const int DHT22_PIN = 33;
const int LDR_PIN = 32;
const int SW520D_PIN = 14;
const int SW18015P_PIN = 13;
const int GUVA_PIN = 35;
const int UMIDADESOLO_PIN = 34;

WiFiClient WOKWI_client;
PubSubClient client(WOKWI_client);
DHT dht(DHT22_PIN, DHT22);
Adafruit_MPU6050 mpu;
Adafruit_BME680 bme(&Wire);

JsonDocument doc;

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20)
  {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi conectado!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nFalha ao conectar ao Wi-Fi. Verifique SSID/senha.");
  }
}

void setup_mpu6050()
{
  while (!mpu.begin(0x68))
  {
    Serial.println("Tentando inicializar MPU6050... Verifique a conexão!");
    delay(2000);
  }
  Serial.println("MPU6050 inicializado com sucesso!");
}

void setup_bme680()
{
  while (!bme.begin())
  {
    Serial.println("Tentando inicializar BME680... Verifique a conexão!");
    delay(2000);
  }
  Serial.println("BME680 inicializado com sucesso!");
}

void wifi_MQTT_Reconnect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi desconectado. Tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
    {
      delay(100);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Conectado!");
    }
    else
    {
      Serial.println("Falha na conexão WiFi.");
    }
  }
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("WOKWI_Client"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(DHT22_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SW520D_PIN, INPUT);
  pinMode(SW18015P_PIN, INPUT);
  pinMode(GUVA_PIN, INPUT);
  pinMode(UMIDADESOLO_PIN, INPUT);

  Serial.println("Iniciando...");

  setup_wifi();
  setup_mpu6050();
  setup_bme680();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  Serial.println("ESP32 inicializado com sucesso!");
}

void LDR_value()
{
  int lux = analogRead(LDR_PIN);

  doc["lux"] = lux;
}

void SW520D_value()
{
  int inclinacao = digitalRead(SW520D_PIN);

  doc["inclinacao"] = inclinacao;
}

void SW18015P_value()
{
  int vibracao = digitalRead(SW18015P_PIN);

  doc["vibracao"] = vibracao;
}

void GUVA_value()
{
  int radiacaoUV = analogRead(GUVA_PIN);

  doc["radiacaoUV"] = radiacaoUV;
}

void UMIDADESOLO_value()
{
  int umidadeSolo = analogRead(UMIDADESOLO_PIN);

  doc["umidadeSolo"] = umidadeSolo;
}

void DHT22_value()
{
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;
}

void BME680_value()
{
  if (! bme.performReading()) {
    Serial.println("Falha na leitura do BME680!");
    return;
  }

  float pressao = bme.pressure / 100.0;
  float altitude = bme.readAltitude(1013.25);

  doc["pressao"] = pressao;
  doc["altitude"] = altitude;
}

void MPU6050_value()
{
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
}

void data_publish()
{
  char buffer[512];
  serializeJson(doc, buffer);

  client.publish("dispositivos/device1/dados", buffer);

  Serial.println("JSON publicado:");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  delay(2500);
}

void loop()
{
  if(!client.connected()){
    wifi_MQTT_Reconnect();
  }

  client.loop(); // <--- ESSENCIAL para MQTT funcionar corretamente
  doc.clear(); // <--- Limpa o JSON antes de preencher novamente

  LDR_value();
  DHT22_value();
  UMIDADESOLO_value();
  GUVA_value();
  BME680_value();
  MPU6050_value();
  SW520D_value();
  SW18015P_value();

  data_publish();
}