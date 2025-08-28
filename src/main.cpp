#define MQTT_MAX_PACKET_SIZE 512

#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h> // Watchdog para evitar crashes

#define LED_STATUS_PIN 23
#define SDA_PIN 21
#define SCL_PIN 22
#define BUZZER_PIN 2
#define DHT11_PIN 33
#define LDR_PIN 32
#define SW520D_PIN 4
#define SW420_PIN 13
#define UMIDADESOLO_PIN 27 // ADC1, evita conflito com WiFi

// Configurações (idealmente em NVS ou WiFiManager)
const char* ssid = "RedmiVlad";
const char* password = "12345678+-/";
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883; // Substitua por 8883 e WiFiClientSecure para TLS em produção

// Timings (non-blocking)
const unsigned long SENSOR_INTERVAL = 10000; // Leitura a cada 10s
const unsigned long LED_INTERVAL = 500;     // Piscar LED
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // Tentativa de reconexão
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;

// Thresholds para alarme (calibrar em campo!)
const float UMIDADE_SOLO_RISCO = 60.0; // % umidade do solo
const int VIBRACAO_RISCO = 1;         // SW420 detectou vibração

bool bmp_ok = false;
bool mpu_ok = false;
bool coletaAtiva = false;
unsigned long lastSensorRead = 0;
unsigned long lastLedBlink = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastWifiReconnect = 0;

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT11_PIN, DHT11);
Adafruit_MPU6050 mpu;
Adafruit_BMP085 bmp;
StaticJsonDocument<512> doc;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi...");
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500); // Único delay permitido no setup
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFalha ao conectar WiFi.");
  }
}

void setup_mpu6050() {
  mpu_ok = mpu.begin();
  Serial.println(mpu_ok ? "✅ MPU6050 OK" : "❌ MPU6050 não encontrado!");
}

void setup_bmp180() {
  bmp_ok = bmp.begin();
  Serial.println(bmp_ok ? "✅ BMP180 OK" : "❌ BMP180 não encontrado!");
}

void reconnect_wifi_mqtt() {
  unsigned long now = millis();
  
  // Reconecta WiFi se desconectado
  if (WiFi.status() != WL_CONNECTED && now - lastWifiReconnect >= WIFI_RECONNECT_INTERVAL) {
    Serial.println("WiFi desconectado. Reconectando...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    lastWifiReconnect = now;
  }

  // Reconecta MQTT se desconectado
  if (WiFi.status() == WL_CONNECTED && !client.connected() && now - lastMqttReconnect >= MQTT_RECONNECT_INTERVAL) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("ESP32_Client_" + String(random(0xffff), HEX))) { // ID único
      Serial.println("Conectado!");
    } else {
      Serial.println("Falha, rc=" + String(client.state()));
    }
    lastMqttReconnect = now;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(5, OUTPUT); // LED WiFi
  pinMode(18, OUTPUT); // LED MQTT
  pinMode(LDR_PIN, INPUT);
  pinMode(SW520D_PIN, INPUT_PULLUP); // Pull-up para debounce
  pinMode(SW420_PIN, INPUT_PULLUP);
  pinMode(UMIDADESOLO_PIN, INPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(5, LOW);
  digitalWrite(18, LOW);

  // Inicializa watchdog (5s timeout)
  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  setup_wifi();
  setup_mpu6050();
  setup_bmp180();
  dht.begin();
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("ESP32 inicializado!");
}

void piscarStatusLED() {
  if (millis() - lastLedBlink >= LED_INTERVAL) {
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50); // Bip curto, único delay mínimo
    digitalWrite(BUZZER_PIN, LOW);
    lastLedBlink = millis();
  }
}

void update_indicators() {
  digitalWrite(5, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  digitalWrite(18, client.connected() ? HIGH : LOW);
}

void ldr_value() {
  int data = analogRead(LDR_PIN);
  float voltage = data * (3.3 / 4095.0);
  float rFixo = 10000;
  float resistance = (voltage * rFixo) / (3.3 - voltage);
  float lux = 500000.0 / resistance; // Calibrar com datasheet do LDR
  doc["lux"] = round(lux * 10) / 10.0; // 1 casa decimal
  coletaAtiva = true;
}

void sw520d_value() {
  int inclinacao = digitalRead(SW520D_PIN);
  doc["inclinacao"] = inclinacao;
  coletaAtiva = true;
}

void sw420_value() {
  int vibracao = digitalRead(SW420_PIN);
  doc["vibracao"] = vibracao;
  coletaAtiva = true;
}

void mpu6050_value() {
  if (!mpu_ok) return;
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
    coletaAtiva = true;
  }
}

void bmp180_value() {
  if (!bmp_ok) return;
  float pressao = bmp.readPressure();
  float altitude = bmp.readAltitude(101500); // Calibrar com pressão local
  doc["pressao"] = pressao;
  doc["altitude"] = altitude;
  coletaAtiva = true;
}

void dht11_value() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();
  if (!isnan(umidade) && !isnan(temperatura)) {
    doc["temperatura"] = round(temperatura * 10) / 10.0;
    doc["umidade"] = round(umidade * 10) / 10.0;
    coletaAtiva = true;
  }
}

void umidade_solo_value() {
  int raw = analogRead(UMIDADESOLO_PIN);
  // Mapeia 4095 (seco) -> 0%, 1000 (molhado) -> 100%. Calibrar em campo!
  float umidadeSolo = map(raw, 4095, 1000, 0, 100);
  umidadeSolo = constrain(umidadeSolo, 0, 100);
  doc["umidadeSolo"] = umidadeSolo;
  coletaAtiva = true;
}

void check_alarms() {
  // Alarme local para risco de deslizamento
  if (doc.containsKey("umidadeSolo") && doc.containsKey("vibracao")) {
    float umidadeSolo = doc["umidadeSolo"];
    int vibracao = doc["vibracao"];
    if (umidadeSolo >= UMIDADE_SOLO_RISCO && vibracao == VIBRACAO_RISCO) {
      digitalWrite(BUZZER_PIN, HIGH); // Alarme contínuo
      Serial.println("⚠️ RISCO DE DESLIZAMENTO DETECTADO!");
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

void data_publish() {
  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  if (n > MQTT_MAX_PACKET_SIZE) {
    Serial.println("⚠️ JSON excede tamanho do buffer!");
    return;
  }
  if (client.connected()) {
    client.publish("monitoramento/encosta", buffer, true); // QoS 1
    Serial.println("JSON publicado:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.println("MQTT desconectado, dados não enviados.");
  }
}

void loop() {
  esp_task_wdt_reset(); // Reseta watchdog
  client.loop(); // Mantém MQTT ativo
  update_indicators();
  reconnect_wifi_mqtt();

  // Leitura de sensores a cada SENSOR_INTERVAL
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    doc.clear();
    coletaAtiva = false;
    
    ldr_value();
    sw520d_value();
    sw420_value();
    mpu6050_value();
    dht11_value();
    bmp180_value();
    umidade_solo_value();
    
    check_alarms();
    data_publish();
    
    if (coletaAtiva) {
      piscarStatusLED();
    }
    
    lastSensorRead = millis();
    
    // Deep sleep para economia de energia (descomente em produção)
    // esp_sleep_enable_timer_wakeup(SENSOR_INTERVAL * 1000);
    // esp_deep_sleep_start();
  }
}