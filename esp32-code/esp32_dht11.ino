// ========================================
// MONITOR DE TEMPERATURA - ESP32-S3 + DHT11
// Envia dados para o backend hospedado na Render
// ========================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// Usa a porta USB nativa do ESP32-S3 para debug
#if defined(ARDUINO_USB_CDC_ON_BOOT)
  #define DEBUG_SERIAL Serial
#else
  #define DEBUG_SERIAL Serial
#endif

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

// Para controle de reconexao e watchdog
unsigned long ultimoChecWiFi = 0;
unsigned long ultimoWiFiOk = 0;

// FUNÇÃO: conecta ao WiFi tentando até conseguir (com limite)
bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("\n--- Conectando ao WiFi ---");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();
  // Tenta por até 30 segundos
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 30000) {
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
    return true;
  } else {
    Serial.println("\nWiFi ainda nao disponivel - tentarei de novo.");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== MONITOR DE TEMPERATURA ESP32-S3 ===");
  Serial.println("Configurando sensor DHT11...");
  dht.begin();
  delay(500);

  Serial.print("SSID configurado: '");
  Serial.print(WIFI_SSID);
  Serial.println("'");

  conectarWiFi();

  // Envia uma leitura imediatamente ao ligar (sem esperar os 60s)
  if (WiFi.status() == WL_CONNECTED) {
    delay(2000);
    lerEEnviar();
  }
}

void loop() {
  unsigned long agora = millis();

  // Atualiza o "último momento com WiFi ok" quando conectado
  if (WiFi.status() == WL_CONNECTED) {
    ultimoWiFiOk = agora;
  }

  // Verifica/reconecta o WiFi se caiu (a cada 5s)
  if (WiFi.status() != WL_CONNECTED && (agora - ultimoChecWiFi > 5000)) {
    ultimoChecWiFi = agora;
    conectarWiFi();
  }

  // Watchdog: se ficar mais de 10 minutos sem WiFi, reinicia a placa
  if (WiFi.status() != WL_CONNECTED && (agora - ultimoWiFiOk > 600000)) {
    Serial.println(">> Sem WiFi por 10 min. Reiniciando o ESP32...");
    delay(2000);
    ESP.restart();
  }

  // Envia leitura no intervalo definido
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

  // Envia para o backend (com tentativas, pois a Render pode estar "dormindo")
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Enviando para: ");
    Serial.println(API_URL);

    bool enviado = false;
    for (int tentativa = 1; tentativa <= 3 && !enviado; tentativa++) {
      if (tentativa > 1) {
        Serial.print("Tentativa ");
        Serial.print(tentativa);
        Serial.println(" de 3...");
        delay(5000); // espera 5s entre tentativas
      }

      HTTPClient http;
      http.setTimeout(60000); // Timeout de 60 segundos (Render pode demorar p/ acordar)
      http.begin(API_URL);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("x-api-key", API_KEY);

      int httpCode = http.POST(json);
      String resposta;

      if (httpCode > 0) {
        resposta = http.getString();
        Serial.print("HTTP Status: ");
        Serial.println(httpCode);
        Serial.print("Resposta do servidor: ");
        Serial.println(resposta);

        if (httpCode == 200 || httpCode == 201) {
          Serial.println(">> SUCESSO: leitura registrada no banco de dados!");
          enviado = true;
        } else if (httpCode == 401) {
          Serial.println(">> ERRO: API_KEY incorreta! Confira o valor no codigo e na Render.");
          enviado = true; // nao adianta tentar de novo
        } else if (httpCode == 404) {
          Serial.println(">> ERRO: URL nao encontrada. Confira API_URL.");
          enviado = true;
        } else if (httpCode == 500) {
          Serial.println(">> ERRO interno no servidor. Veja os logs na Render.");
        }
      } else {
        Serial.print("ERRO na conexao HTTP. Codigo: ");
        Serial.println(httpCode);
        switch (httpCode) {
          case HTTPC_ERROR_CONNECTION_REFUSED:
            Serial.println("Conexao recusada - servidor pode estar dormindo na Render. Tentando de novo...");
            break;
          case HTTPC_ERROR_CONNECTION_LOST:
            Serial.println("Conexao perdida durante o envio.");
            break;
          case HTTPC_ERROR_READ_TIMEOUT:
            Serial.println("Timeout de leitura (servidor demorou a responder).");
            break;
          case HTTPC_ERROR_NOT_CONNECTED:
            Serial.println("Nao foi possivel conectar ao servidor (verifique SSL/URL e internet).");
            break;
          default:
            Serial.println("Verifique a URL e a conexao com a internet.");
            break;
        }
      }
      http.end();
    }
  } else {
    Serial.println("WiFi desconectado - reconexao sera feita pelo loop.");
  }
}
