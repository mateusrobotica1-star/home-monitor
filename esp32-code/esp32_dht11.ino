// ========================================
// MONITOR DE TEMPERATURA - ESP32-S3 + DHT11/DHT22
// ========================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "esp_task_wdt.h"

const char* WIFI_SSID = "Mateus";
const char* WIFI_PASSWORD = "mateus2000";
const char* API_URL = "https://home-monitor-backend.onrender.com/api/temperatura";
const char* API_KEY = "1";
const char* BUZZER_URL = "https://home-monitor-backend.onrender.com/api/buzzer";

#define DHTPIN 4
#define DHTTYPE DHT11
#define SOM_PIN 5
#define SOM_LIMITE_ALERTA 300
#define BUZZER_PIN 12

DHT dht(DHTPIN, DHTTYPE);

const unsigned long INTERVALO_SOM = 1500;
const unsigned long INTERVALO_DHT = 10000;
const unsigned long INTERVALO_KEEPALIVE = 30000;
const unsigned long INTERVALO_BUZZER = 5000;
const unsigned long INTERVALO_WIFI = 5000;

const float VAR_TEMP = 0.1;
const float VAR_UMID = 0.5;
const int   VAR_SOM = 10;

float ultTempEnviada = -999;
float ultUmidEnviada = -999;
int   ultSomEnviado = -999;
float ultTempLida = -999;
float ultUmidLida = -999;

unsigned long timerSom = 0;
unsigned long timerDHT = 0;
unsigned long timerKeepAlive = 0;
unsigned long timerBuzzer = 0;
unsigned long timerWifi = 0;
unsigned long ultimoAtividade = 0;

bool buzzerLigado = false;

int lerNivelSom() {
  int maximo = 0;
  unsigned long fim = millis() + 50;
  while (millis() < fim) {
    int v = analogRead(SOM_PIN);
    if (v > maximo) maximo = v;
  }
  return maximo;
}

bool lerDHT() {
  float t = dht.readTemperature();
  float u = dht.readHumidity();
  if (!isnan(t) && !isnan(u)) {
    ultTempLida = t;
    ultUmidLida = u;
    return true;
  }
  return false;
}

void enviarDados() {
  float temperatura = ultTempLida;
  float umidade = ultUmidLida;

  Serial.println("\n=== Nova leitura ===");
  Serial.printf("Temp: %.1fC | Umid: %.1f%%\n", temperatura, umidade);

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println(">> Sensor NaN - pulando envio");
    return;
  }

  int nivelSom = lerNivelSom();
  Serial.printf("Som: %d (limite: %d)\n", nivelSom, SOM_LIMITE_ALERTA);

  String json = "{\"temperatura\":" + String(temperatura, 1) +
                ",\"umidade\":" + String(umidade, 1) +
                ",\"som\":" + String(nivelSom) + "}";

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi off - pulando envio");
    return;
  }

  HTTPClient http;
  http.setTimeout(30000);
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);

  int httpCode = http.POST(json);
  if (httpCode == 200 || httpCode == 201) {
    Serial.println(">> Enviado com sucesso!");
    ultTempEnviada = temperatura;
    ultUmidEnviada = umidade;
    ultSomEnviado = nivelSom;
  } else if (httpCode > 0) {
    Serial.printf(">> Erro HTTP %d\n", httpCode);
  } else {
    Serial.println(">> Falha na conexao");
  }
  http.end();
  ultimoAtividade = millis();
}

void consultarBuzzer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(BUZZER_URL);
  http.addHeader("x-api-key", API_KEY);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String resp = http.getString();
    buzzerLigado = resp.indexOf("\"ligado\":true") >= 0;
    Serial.printf("Buzzer: %s\n", buzzerLigado ? "LIGADO" : "desligado");
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  // *** DESATIVA O WATCHDOG DO WIFI ***
  // O WiFi roda no Core 1 e o HTTPClient bloqueia o loop,
  // causando crash. Desativamos o TWDT do loop task.
  esp_task_wdt_delete(NULL);

  Serial.println("\n=== MONITOR DE TEMPERATURA ===");
  Serial.printf("Sensor: GPIO%d\n", DHTPIN);
  Serial.println("VCC->3.3V  DATA->GPIO4  GND->GND");

  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi");

  unsigned long inicioWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioWifi < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK - IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi falhou - tentara no loop");
  }

  ultimoAtividade = millis();
  Serial.println("=== Iniciando monitoramento ===");
}

void loop() {
  unsigned long agora = millis();

  // ---- WiFi: reconecta se caiu ----
  if (WiFi.status() != WL_CONNECTED && (agora - timerWifi > INTERVALO_WIFI)) {
    timerWifi = agora;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // Watchdog proprio: reinicia se 5 minutos sem enviar nada
  if (agora - ultimoAtividade > 300000) {
    Serial.println(">> 5 min sem atividade - reiniciando");
    ESP.restart();
  }

  // ---- Som: a cada 1.5s ----
  if (agora - timerSom >= INTERVALO_SOM) {
    timerSom = agora;
    int som = lerNivelSom();
    if (abs(som - ultSomEnviado) >= VAR_SOM) {
      Serial.printf("Som: %d -> %d\n", ultSomEnviado, som);
      ultSomEnviado = som;
      enviarDados();
    }
  }

  // ---- DHT: a cada 10s ----
  if (agora - timerDHT >= INTERVALO_DHT) {
    timerDHT = agora;
    if (lerDHT()) {
      float t = ultTempLida;
      float u = ultUmidLida;
      bool mudouT = fabs(t - ultTempEnviada) >= VAR_TEMP;
      bool mudouU = fabs(u - ultUmidEnviada) >= VAR_UMID;
      if (mudouT || mudouU) {
        Serial.printf("DHT: %.1fC / %.1f%%\n", t, u);
        enviarDados();
      }
    } else {
      Serial.println(">> DHT: falha na leitura");
    }
  }

  // ---- Keep-alive: a cada 30s ----
  if (agora - timerKeepAlive >= INTERVALO_KEEPALIVE) {
    timerKeepAlive = agora;
    enviarDados();
  }

  // ---- Buzzer: a cada 5s ----
  if (agora - timerBuzzer >= INTERVALO_BUZZER) {
    timerBuzzer = agora;
    consultarBuzzer();
  }

  // ---- Controle do buzzer ----
  if (buzzerLigado) {
    ledcAttach(BUZZER_PIN, 3000, 8);
    ledcWrite(BUZZER_PIN, 240);
  } else {
    ledcDetach(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(10);
}
