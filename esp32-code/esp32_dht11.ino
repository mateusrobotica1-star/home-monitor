// ========================================
// MONITOR DE TEMPERATURA - ESP32-S3 + DHT11
// Envia dados para o backend hospedado na Render
// ========================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <math.h>
#include <stdlib.h>

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

// Pin do sensor de som KY-037 (saída analógica AOUT/DO)
// GPIO5 é um pino ADC1 válido no ESP32-S3, separado do GPIO4 (DHT11)
#define SOM_PIN 5

// Limite de alerta de som (ajuste conforme sua sensibilidade)
// Valores tipicos: silencio ~10-50, barulho alto ~200-1000+
#define SOM_LIMITE_ALERTA 300

DHT dht(DHTPIN, DHTTYPE);

// Função para medir o nível de som (0-4095)
int lerNivelSom() {
  int maxLeitura = 0;
  // Amostra por 50ms para capturar picos de som
  unsigned long fim = millis() + 50;
  while (millis() < fim) {
    int v = analogRead(SOM_PIN);
    if (v > maxLeitura) {
      maxLeitura = v;
    }
    delay(1);
  }
  return maxLeitura;
}

// Intervalos de verificação
const unsigned long CHECAR_SOM_INTERVALO = 1500;   // verifica som a cada 1,5s
const unsigned long CHECAR_DHT_INTERVALO = 10000;  // verifica DHT11 a cada 10s
const unsigned long AUTO_ENVIO_INTERVALO = 30000;  // envia sempre a cada 30s (keep-alive)

// Limiar de variação para enviar imediatamente
const float VAR_TEMP_LIMITE = 0.1;   // envia se temperatura mudar >= 0.1°C
const float VAR_UMID_LIMITE = 1.0;   // envia se umidade mudar >= 1%
const int   VAR_SOM_LIMITE = 20;     // envia se o som variar >= 20

// Últimos valores enviados
float ultTempEnviada = -999;
float ultUmidEnviada = -999;
int   ultSomEnviado = -999;

// Controle de tempo
unsigned long ultimoCheckSom = 0;
unsigned long ultimoCheckDHT = 0;
unsigned long ultimoEnvioAutomatico = 0;

// Para controle de reconexao e watchdog
unsigned long ultimoChecWiFi = 0;
unsigned long ultimoWiFiOk = 0;

// Estado atual (mais recente lido)
float ultTempLida = -999;
float ultUmidLida = -999;
int   ultSomLido = 0;

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

  // Envia uma leitura imediatamente ao ligar
  if (WiFi.status() == WL_CONNECTED) {
    delay(2000);
    lerEAtualizarReferencia();
    enviarDados();
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

  // ---- Verifica o som com frequência (quase instantâneo) ----
  if (agora - ultimoCheckSom >= CHECAR_SOM_INTERVALO) {
    ultimoCheckSom = agora;
    int somAtual = lerNivelSom();
    ultSomLido = somAtual;

    // Se o som mudou o suficiente, envia imediatamente
    if (abs(somAtual - ultSomEnviado) >= VAR_SOM_LIMITE) {
      Serial.printf("Som mudou: %d -> %d. Enviando...\n", ultSomEnviado, somAtual);
      enviarDados();
      ultSomEnviado = somAtual;
    }
  }

  // ---- Verifica o DHT11 com frequência menor (temp/umidade) ----
  if (agora - ultimoCheckDHT >= CHECAR_DHT_INTERVALO) {
    ultimoCheckDHT = agora;

    float temp = dht.readTemperature();
    float umid = dht.readHumidity();

    if (!isnan(temp) && !isnan(umid)) {
      ultTempLida = temp;
      ultUmidLida = umid;

      bool mudouTemp = fabs(temp - ultTempEnviada) >= VAR_TEMP_LIMITE;
      bool mudouUmid = fabs(umid - ultUmidEnviada) >= VAR_UMID_LIMITE;

      // Se algum valor mudou, envia imediatamente
      if (mudouTemp || mudouUmid) {
        Serial.printf("Temp/umid mudou: %.1fC/%.1f%%. Enviando...\n", temp, umid);
        enviarDados();
        ultTempEnviada = temp;
        ultUmidEnviada = umid;
      }
    }
  }

  // ---- Envia periodicamente mesmo sem mudanças (keep-alive) ----
  if (agora - ultimoEnvioAutomatico >= AUTO_ENVIO_INTERVALO) {
    ultimoEnvioAutomatico = agora;
    lerEAtualizarReferencia();
    enviarDados();
  }

  delay(200);
}

// Atualiza as referências (para não enviar por variação logo após o envio periódico)
void lerEAtualizarReferencia() {
  ultTempLida = dht.readTemperature();
  ultUmidLida = dht.readHumidity();
  ultSomLido = lerNivelSom();
  if (!isnan(ultTempLida)) ultTempEnviada = ultTempLida;
  if (!isnan(ultUmidLida)) ultUmidEnviada = ultUmidLida;
  ultSomEnviado = ultSomLido;
}

void enviarDados() {
  // Usa os valores mais recentes lidos
  float temperatura = ultTempLida;
  float umidade = ultUmidLida;

  Serial.println("\n=== Nova leitura ===");
  Serial.printf("Temperatura: %.1f C | Umidade: %.1f%%\n", temperatura, umidade);

  // Lê o nível de som em tempo real
  int nivelSom = lerNivelSom();
  ultSomLido = nivelSom;
  Serial.printf("Nivel de som: %d (limite: %d)\n", nivelSom, SOM_LIMITE_ALERTA);
  if (nivelSom > SOM_LIMITE_ALERTA) {
    Serial.println(">> ALERTA: som alto detectado!");
  }

  // Monta o JSON a ser enviado (incluindo o nível de som)
  String json = "{\"temperatura\":" + String(temperatura, 1) +
                ",\"umidade\":" + String(umidade, 1) +
                ",\"som\":" + String(nivelSom) + "}";
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
