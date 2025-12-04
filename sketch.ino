#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---------------------------------------------------
// CONFIGURAÇÃO DO DISPLAY OLED
// ---------------------------------------------------
#define OLED_W 128
#define OLED_H 64
#define OLED_RST -1

Adafruit_SSD1306 visor(OLED_W, OLED_H, &Wire, OLED_RST);

// ---------------------------------------------------
// DEFINIÇÃO DOS PINOS
// ---------------------------------------------------
#define PIN_SENSOR 35
#define PIN_INDICADOR 2
#define PIN_SOM 25

// ---------------------------------------------------
// LIMITES DO BATIMENTO CARDÍACO
// ---------------------------------------------------
int faixaMin = 50;
int faixaMax = 120;

// ---------------------------------------------------
// CONFIG Wi-Fi E MQTT
// ---------------------------------------------------
const char* ssidWiFi = "Wokwi-GUEST";
const char* senhaWiFi = "";
const char* servidorMQTT = "test.mosquitto.org";

WiFiClient clientWiFi;
PubSubClient clienteMQTT(clientWiFi);

// ---------------------------------------------------
// FUNÇÃO: INICIALIZAR Wi-Fi
// ---------------------------------------------------
void conectarWiFi() {
  Serial.println("Conectando à rede Wi-Fi...");

  WiFi.begin(ssidWiFi, senhaWiFi);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nWi-Fi online!");
}

// ---------------------------------------------------
// FUNÇÃO: RECONEXÃO MQTT
// ---------------------------------------------------
void reconnectMQTT() {
  while (!clienteMQTT.connected()) {
    Serial.println("Tentando contato com servidor MQTT...");

    if (clienteMQTT.connect("ESP32_HeartMonitor")) {
      Serial.println("Conectado ao broker!");
    } else {
      Serial.print("Falha: ");
      Serial.println(clienteMQTT.state());
      Serial.println("Aguardando nova tentativa...");
      delay(3000);
    }
  }
}

// ---------------------------------------------------
// FUNÇÃO: ALERTA AUDITIVO
// ---------------------------------------------------
void emitirAlerta() {
  for (int i = 0; i < 3; i++) {
    tone(PIN_SOM, 1000);
    delay(150);
    noTone(PIN_SOM);
    delay(150);
  }
}

// ---------------------------------------------------
// SETUP
// ---------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_INDICADOR, OUTPUT);
  digitalWrite(PIN_INDICADOR, LOW);

  if (!visor.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Falha: OLED não encontrado.");
    while (true);
  }

  visor.clearDisplay();
  visor.setTextColor(SSD1306_WHITE);
  visor.setTextSize(1);
  visor.setCursor(0, 20);
  visor.println("Iniciando sistema...");
  visor.display();
  delay(1200);

  conectarWiFi();
  clienteMQTT.setServer(servidorMQTT, 1883);
}

// ---------------------------------------------------
// LOOP PRINCIPAL
// ---------------------------------------------------
void loop() {
  if (!clienteMQTT.connected()) {
    reconnectMQTT();
  }
  clienteMQTT.loop();

  // Simulação de captura do sensor
  int leitura = analogRead(PIN_SENSOR);
  int bpm = map(leitura, 300, 600, 40, 180);
  bpm = constrain(bpm, 0, 200);

  String categoria = "";

  // ---------------------------------------------------
  // CLASSIFICAÇÃO
  // ---------------------------------------------------
  if (bpm < faixaMin) {
    categoria = "abaixo";

    visor.clearDisplay();
    visor.setCursor(0, 0);
    visor.println("Atencao!");
    visor.println("Batimento muito baixo");
    visor.setCursor(0, 25);
    visor.print("BPM: ");
    visor.println(bpm);
    visor.display();

    digitalWrite(PIN_INDICADOR, HIGH);
    emitirAlerta();
  }
  else if (bpm > faixaMax) {
    categoria = "acima";

    visor.clearDisplay();
    visor.setCursor(0, 0);
    visor.println("Cuidado!");
    visor.println("Batimento elevado");
    visor.setCursor(0, 25);
    visor.print("BPM: ");
    visor.println(bpm);
    visor.display();

    digitalWrite(PIN_INDICADOR, HIGH);
    emitirAlerta();
  }
  else {
    categoria = "estavel";

    visor.clearDisplay();
    visor.setCursor(0, 0);
    visor.println("Frequencia OK");
    visor.setCursor(0, 20);
    visor.print("Atual: ");
    visor.print(bpm);
    visor.println(" bpm");
    visor.display();

    digitalWrite(PIN_INDICADOR, LOW);
    noTone(PIN_SOM);
  }

  // Log no serial
  Serial.print("BPM lido: ");
  Serial.print(bpm);
  Serial.print(" | Estado: ");
  Serial.println(categoria);

  // ---------------------------------------------------
  // ENVIO MQTT
  // ---------------------------------------------------
  char bufferBPM[10];
  char bufferEstado[20];
  sprintf(bufferBPM, "%d", bpm);
  categoria.toCharArray(bufferEstado, 20);

  clienteMQTT.publish("cardio/batimento", bufferBPM);
  clienteMQTT.publish("cardio/estado", bufferEstado);

  delay(2000);
}