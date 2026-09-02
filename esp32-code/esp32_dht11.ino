// ========================================
// MONITOR DE TEMPERATURA - ESP32-S3 + DHT11
// Envia dados para o backend hospedado na Render
// ========================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// CONFIGURAÇÕES DO WIFI - ALTERE AQUI!
const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";

// URL DO SEU BACKEND NA RENDER - ALTERE AQUI!
// Exemplo: https://meu-monitor.onrender.com/api/temperatura
const char* API_URL = "https://SEU-BACKEND.onrender.com/api/temperatura";

// SENHA/TOKEN PARA PROTEGER O ENDPOINT - CRIE UM TOKEN SEU
const char* API_KEY = "sua_chave_secreta_aqui";

// Pin do DHT11 (default GPIO4 no ESP32-S3)
#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// Intervalo de envio em segundos (60 = 1 minuto)
const unsigned long ENVIO_INTERVALO = 60;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== MONITOR DE TEMPERATURA ESP32-S3 ===");
  dht.begin();

  // Conecta ao WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoEnvio >= ENVIO_INTERVALO * 1000) {
    ultimoEnvio = agora;
    lerEEnviar();
  }

  delay(1000);
}

void lerEEnviar() {
  // Lê o sensor
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  // Se o sensor falhar, tenta de novo
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler o sensor DHT11! Tentando novamente...");
    delay(2000);
    temperatura = dht.readTemperature();
    umidade = dht.readHumidity();
    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("Falha definitiva ao ler o sensor.");
      return;
    }
  }

  Serial.printf("Temperatura: %.1f C | Umidade: %.1f%%\n", temperatura, umidade);

  // Monta o JSON a ser enviado
  String json = "{\"temperatura\":" + String(temperatura, 1) +
                ",\"umidade\":" + String(umidade, 1) + "}";

  // Envia para o backend
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_KEY);

    int httpCode = http.POST(json);

    if (httpCode > 0) {
      Serial.printf("Resposta do servidor: %d\n", httpCode);
      Serial.println(http.getString());
    } else {
      Serial.printf("Erro na conexão HTTP: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi desconectado, tentando reconectar...");
    WiFi.reconnect();
  }
}
