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

#define LED_STATUS_PIN 23 // led funcionamento do sistema(coleta de dados geral)
#define SDA_PIN 21 // i2c
#define SCL_PIN 22 // i2c
#define BUZZER_PIN  2 //buzzer

const char* ssid = "RedmiVlad";
const char* password = "12345678+-/";
const char* mqtt_server = "test.mosquitto.org";


const int DHT11_PIN = 33;
const int LDR_PIN = 32;
const int SW520D_PIN = 4;
const int SW420_PIN = 13;
const int UMIDADESOLO_PIN = 34; //mudança visto que o pino anterior cruzava com o uso do wifi // Significa que o GPIO12 (que está conectado ao sensor de umidade do solo) está ligado no ADC2, e no ESP32, o ADC2 não pode ser usado simultaneamente com o Wi-Fi ativo. É uma limitação da própria arquitetura do ESP32 quando o Wi-Fi está operando no modo station (STA).


bool bmp_ok = false;
bool mpu_ok = false;
bool coletaAtiva = false; //variável bool coletaAtiva que fica true quando pelo menos um sensor coleta e atualiza seu valor no doc, e com base nessa variável a função piscarStatusLED() seria chamada ou não


WiFiClient WOKWI_client;
PubSubClient client(WOKWI_client);
DHT dht(DHT11_PIN, DHT11);
Adafruit_MPU6050 mpu;
Adafruit_BMP085 bmp;


StaticJsonDocument<512> doc;


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
  mpu_ok = mpu.begin();
  if (mpu_ok) {
    Serial.println("✅ MPU6050 inicializado com sucesso!");
  } else {
    Serial.println("❌ MPU6050 não encontrado!");
  }
}


void setup_bmp180() { 
  bmp_ok = bmp.begin();
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
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(18, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SW520D_PIN, INPUT);
  pinMode(SW420_PIN, INPUT);
  pinMode(UMIDADESOLO_PIN, INPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);  //led indicador do funcionamento de coleta de dados
  pinMode(BUZZER_PIN, OUTPUT); // buzer indicador de funcionamento
  
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Iniciando...");


  setup_wifi();
  setup_mpu6050();
  setup_bmp180();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  Serial.println("🚀 ESP32 inicializado com sucesso!");
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


void LDR_value() { //SENSOR DE LUMINOSIDADE
  int data = analogRead(LDR_PIN);
  float voltage = data * (3.3 / 4095.0);
  float rFixo = 10000;
  float resistance = (voltage * rFixo) / (3.3 - voltage);
  float lux = 500000.0 / resistance;


  doc["lux"] = lux;
  coletaAtiva = true;
}


void SW520D_value() { // SENSOR DE VIBRAÇÃO
  int inclinacao = digitalRead(SW520D_PIN);


  doc["inclinação"] = inclinacao;
  coletaAtiva = true;
}


void SW420_value() {// SENSOR DE ONCLINAÇÃO
  int vibracao = digitalRead(SW420_PIN);


  doc["vibracao"] = vibracao;
  coletaAtiva = true;
}


void MPU6050_value() { // SENSOR GISROSCOPIO
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
  coletaAtiva = true;
}

void DHT11_value() { // SENSOR DE TEMPERATURA E UMIDADE DO AR
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;
  coletaAtiva = true;
}


void BMP180_value() { // SENSOR DE PRESÃO/ALTURA
  float pressao = bmp.readPressure();
  float altitude = bmp.readAltitude(101500);
  doc["pressao"] = pressao;
  doc["altitude"] = altitude;
  coletaAtiva = true;
}

void UMIDADESOLO_value() { // SENSOR DE UMIDADE DO SOLO
  float umidadeSolo = analogRead(UMIDADESOLO_PIN);

  doc["umidadeSolo"] = umidadeSolo;
  coletaAtiva = true;
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
  
  wifiReconnect(); //colocar estrutura if para não entrar no reconect quando estiver conectado
  
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


  data_publish();

  if (coletaAtiva) {
  piscarStatusLED();
  coletaAtiva = false;  // reseta para próxima iteração
}
}