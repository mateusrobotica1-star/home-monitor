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

// URL DO SEU BACKEND NA RENDER - JÁ APONTA PARA O SEU!
const char* API_URL = "https://home-monitor-backend.onrender.com/api/temperatura";

// SENHA/TOKEN PARA PROTEGER O ENDPOINT - USE A MESMA DA RENDER!
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
  Serial.println("Configurando sensor DHT11...");
  dht.begin();
  delay(500);

  // Mostra o WiFi disponível
  Serial.println("\n--- Verificando WiFi ---");
  Serial.print("SSID configurado: '");
  Serial.print(WIFI_SSID);
  Serial.println("'");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");

  // Timeout de 15 segundos para conectar
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("Forca do sinal (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("---------------------------------");
  } else {
    Serial.println("\nFALHA: Nao consegui conectar ao WiFi!");
    Serial.println("Verifique WIFI_SSID e WIFI_PASSWORD no codigo.");
  }
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
  Serial.println("\n=== Nova leitura ===");

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
      Serial.println("Verifique a fiação: VCC->3.3V, DATA->GPIO4, GND->GND");
      return;
    }
  }

  Serial.printf("Temperatura: %.1f C | Umidade: %.1f%%\n", temperatura, umidade);

  // Monta o JSON a ser enviado
  String json = "{\"temperatura\":" + String(temperatura, 1) +
                ",\"umidade\":" + String(umidade, 1) + "}";
  Serial.print("JSON enviado: ");
  Serial.println(json);

  // Envia para o backend
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Enviando para: ");
    Serial.println(API_URL);

    HTTPClient http;
    http.setTimeout(20000); // Timeout de 20 segundos
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_KEY);

    int httpCode = http.POST(json);

    if (httpCode > 0) {
      String resposta = http.getString();
      Serial.print("HTTP Status: ");
      Serial.println(httpCode);
      Serial.print("Resposta do servidor: ");
      Serial.println(resposta);

      // Interpreta os códigos de erro comuns
      if (httpCode == 200 || httpCode == 201) {
        Serial.println(">> SUCESSO: leitura registrada no banco de dados!");
      } else if (httpCode == 401) {
        Serial.println(">> ERRO: API_KEY incorreta! Confira o valor no codigo e na Render.");
      } else if (httpCode == 404) {
        Serial.println(">> ERRO: URL nao encontrada. Confira API_URL.");
      } else if (httpCode == 500) {
        Serial.println(">> ERRO interno no servidor. Veja os logs na Render.");
      }
    } else {
      Serial.print("ERRO na conexao HTTP. Codigo: ");
      Serial.println(httpCode);
      switch (httpCode) {
        case HTTPC_ERROR_CONNECTION_REFUSED:
          Serial.println("Conexao recusada - servidor indisponivel (pode estar dormindo na Render, aguarde e tente de novo).");
          break;
        case HTTPC_ERROR_CONNECTION_LOST:
          Serial.println("Conexao perdida durante o envio.");
          break;
        case HTTPC_ERROR_READ_TIMEOUT:
          Serial.println("Timeout de leitura (servidor demorou a responder).");
          break;
        case HTTPC_ERROR_SSL_ACQUIRE:
        case HTTPC_ERROR_SSL_CONNECTED:
          Serial.println("Erro de SSL/TLS na conexao.");
          break;
        default:
          Serial.println("Verifique a URL e a conexao com a internet.");
          break;
      }
    }
    http.end();
  } else {
    Serial.println("WiFi desconectado, tentando reconectar...");
    WiFi.reconnect();
  }
}
