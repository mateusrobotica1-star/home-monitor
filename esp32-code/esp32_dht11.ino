// ========================================
// MONITOR DE CASA - ESP32-S3
// DHT11/22 + Som + Ultrassonico + Buzzer
// ========================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// === WIFI E SERVIDOR ===
const char* WIFI_SSID = "Mateus";
const char* WIFI_PASSWORD = "mateus2000";
const char* API_URL = "https://home-monitor-backend.onrender.com/api/temperatura";
const char* API_KEY = "1";
const char* BUZZER_URL = "https://home-monitor-backend.onrender.com/api/buzzer";
const char* RECALIBRAR_URL = "https://home-monitor-backend.onrender.com/api/recalibrar";
const char* RECALIBRAR_DONE_URL = "https://home-monitor-backend.onrender.com/api/recalibrar/done";

// === PINOS ===
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOM_PIN 5
#define BUZZER_PIN 12
#define TRIG_PIN 6        // Ultrassonico Trig
#define ECHO_PIN 7        // Ultrassonico Echo

DHT dht(DHTPIN, DHTTYPE);

// === LIMITES ===
#define SOM_LIMITE_ALERTA 300
#define MOVIMENTO_TOLERANCIA 5    // cm

// === INTERVALOS (ms) ===
#define INTERVALO_SOM 1500
#define INTERVALO_DHT 10000
#define INTERVALO_KEEPALIVE 30000
#define INTERVALO_BUZZER 5000
#define INTERVALO_MOVIMENTO 500

// === SENSOR DE SOM ===
int lerNivelSom() {
  int maximo = 0;
  unsigned long fim = millis() + 50;
  while (millis() < fim) {
    int v = analogRead(SOM_PIN);
    if (v > maximo) maximo = v;
  }
  return maximo;
}

// === SENSOR ULTRASSONICO (HC-SR04) ===
long lerDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH, 20000);
  if (duracao == 0) return -1;
  return duracao * 0.0343 / 2;
}

long lerDistanciaMedia() {
  long soma = 0;
  int validas = 0;
  for (int i = 0; i < 5; i++) {
    long d = lerDistancia();
    Serial.printf("  Leitura %d: %ld cm\n", i + 1, d);
    if (d > 0) { soma += d; validas++; }
    delay(50);
  }
  if (validas == 0) {
    Serial.println("  NENHUMA leitura valida! Sensor desconectado ou pinos errados.");
    return -1;  // retorna -1 ao inves de 30 para diagnosticar
  }
  return soma / validas;
}

// === SENSOR DHT ===
float ultTempEnviada = -999;
float ultUmidEnviada = -999;
int   ultSomEnviado = -999;
float ultTempLida = -999;
float ultUmidLida = -999;

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

// === ULTRASSONICO - ESTADO ===
long distanciaBase = -1;    // -1 = nao calibrado
bool movimentoDetectado = false;

// === TIMERS ===
unsigned long timerSom = 0;
unsigned long timerDHT = 0;
unsigned long timerKeepAlive = 0;
unsigned long timerBuzzer = 0;
unsigned long timerMovimento = 0;
unsigned long timerWifi = 0;
unsigned long ultimoAtividade = 0;

bool buzzerLigado = false;

// === CALIBRAR SENSOR ===
void calibrarSensor() {
  Serial.println("\n>>> CALIBRANDO SENSOR DE MOVIMENTO <<<");
  Serial.println("Certifique-se que NAO ha ninguem na frente do sensor!");
  delay(2000);

  distanciaBase = lerDistanciaMedia();

  if (distanciaBase <= 0) {
    Serial.println(">> FALHA NA CALIBRACAO! Sensor nao respondeu.");
    Serial.println(">> Verifique: Trig->GPIO6, Echo->GPIO7, VCC->5V, GND->GND");
    Serial.println(">> Se GPIO6 nao funcionar, troque Trig para GPIO15.");
  } else {
    Serial.printf(">> Calibracao OK! Distancia base: %ld cm\n", distanciaBase);
  }

  // Envia distancia base pro servidor
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(15000);
    http.begin(RECALIBRAR_DONE_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_KEY);
    String json = "{\"distancia\":" + String(distanciaBase) + "}";
    int httpCode = http.POST(json);
    if (httpCode == 200 || httpCode == 201) {
      Serial.println(">> Distancia base enviada ao servidor!");
    }
    http.end();
  }
}

// === CONSULTAR RECALIBRAR ===
void consultarRecalibrar() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(RECALIBRAR_URL);
  http.addHeader("x-api-key", API_KEY);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String resp = http.getString();
    if (resp.indexOf("\"pendente\":true") >= 0) {
      Serial.println("\n>> Servidor pediu RECALIBRACAO!");
      calibrarSensor();
      return;
    }
  }
  http.end();
}

// === ENVIAR DADOS AO SERVIDOR ===
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

  // Le distancia atual para mostrar no serial
  long distAtual = lerDistancia();
  if (distAtual > 0 && distanciaBase > 0) {
    long diff = abs(distAtual - distanciaBase);
    Serial.printf("Dist: %ld cm (base: %ld, diff: %ld, limite: %d)\n",
                  distAtual, distanciaBase, diff, MOVIMENTO_TOLERANCIA);
  } else if (distAtual <= 0) {
    Serial.println("Dist: ERRO (sensor nao responde)");
  }

  Serial.printf("Movimento: %s\n", movimentoDetectado ? "SIM" : "NAO");

  String json = "{\"temperatura\":" + String(temperatura, 1) +
                ",\"umidade\":" + String(umidade, 1) +
                ",\"som\":" + String(nivelSom) +
                ",\"movimento\":" + String(movimentoDetectado ? "true" : "false") + "}";

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

// === CONSULTA BUZZER ===
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

// === SETUP ===
void setup() {
  Serial.begin(115200);

  Serial.println("\n=== MONITOR DE CASA ===");
  Serial.println("Sensores: DHT + Som + Ultrassonico + Buzzer");
  Serial.printf("DHT: GPIO%d | Som: GPIO%d | Buzzer: GPIO%d\n", DHTPIN, SOM_PIN, BUZZER_PIN);
  Serial.printf("Ultrassonico: Trig=GPIO%d Echo=GPIO%d\n", TRIG_PIN, ECHO_PIN);

  dht.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

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

  // CALIBRACAO INICIAL DO ULTRASSONICO
  calibrarSensor();

  ultimoAtividade = millis();
  Serial.println("\n=== Monitoramento iniciado! ===\n");
}

// === LOOP ===
void loop() {
  unsigned long agora = millis();

  // WiFi reconexao
  if (WiFi.status() != WL_CONNECTED && (agora - timerWifi > 5000)) {
    timerWifi = agora;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // Watchdog: 5 min sem enviar = reinicia
  if (agora - ultimoAtividade > 300000) {
    Serial.println(">> 5 min sem atividade - reiniciando");
    ESP.restart();
  }

  // Som: a cada 1.5s
  if (agora - timerSom >= INTERVALO_SOM) {
    timerSom = agora;
    int som = lerNivelSom();
    if (abs(som - ultSomEnviado) >= 10) {
      Serial.printf("Som: %d -> %d\n", ultSomEnviado, som);
      ultSomEnviado = som;
      enviarDados();
    }
  }

  // DHT: a cada 10s
  if (agora - timerDHT >= INTERVALO_DHT) {
    timerDHT = agora;
    if (lerDHT()) {
      float t = ultTempLida;
      float u = ultUmidLida;
      if (fabs(t - ultTempEnviada) >= 0.1 || fabs(u - ultUmidEnviada) >= 0.5) {
        Serial.printf("DHT: %.1fC / %.1f%%\n", t, u);
        enviarDados();
      }
    } else {
      Serial.println(">> DHT: falha na leitura");
    }
  }

  // Ultrassonico (movimento): a cada 0.5s
  if (agora - timerMovimento >= INTERVALO_MOVIMENTO) {
    timerMovimento = agora;

    // So detecta se calibracao OK
    if (distanciaBase > 0) {
      long dist = lerDistancia();
      if (dist > 0) {
        long diferenca = abs(dist - distanciaBase);
        bool movimentoNovo = (diferenca >= MOVIMENTO_TOLERANCIA);
        if (movimentoNovo != movimentoDetectado) {
          movimentoDetectado = movimentoNovo;
          if (movimentoDetectado) {
            Serial.printf(">> MOVIMENTO! Dist: %ld cm (base: %ld)\n", dist, distanciaBase);
          } else {
            Serial.printf(">> Ambiente livre. Dist: %ld cm\n", dist);
          }
          enviarDados();
        }
      }
    } else {
      // Sensor nao calibrado - avisa a cada 10s
      static unsigned long ultAviso = 0;
      if (agora - ultAviso > 10000) {
        ultAviso = agora;
        Serial.println(">> Sensor nao calibrado! Calibre pelo site ou reinicie o ESP32.");
      }
    }
  }

  // Keep-alive: a cada 30s
  if (agora - timerKeepAlive >= INTERVALO_KEEPALIVE) {
    timerKeepAlive = agora;
    enviarDados();
  }

  // Buzzer: a cada 5s
  if (agora - timerBuzzer >= INTERVALO_BUZZER) {
    timerBuzzer = agora;
    consultarBuzzer();
    consultarRecalibrar();  // checa se site pediu recalibracao
  }

  // Controle do buzzer
  if (buzzerLigado) {
    ledcAttach(BUZZER_PIN, 3000, 8);
    ledcWrite(BUZZER_PIN, 240);
  } else {
    ledcDetach(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(10);
}
