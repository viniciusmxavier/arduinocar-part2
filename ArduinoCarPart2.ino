// ----------------------------
// INCLUDES
// ----------------------------
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ----------------------------
// MOTORES - PONTE H
// ----------------------------
#define IN1_ESQUERDO 4
#define IN2_ESQUERDO 8
#define PWM_ESQ 5   // PWM

#define IN3_DIREITO 3
#define IN4_DIREITO 9
#define PWM_DIR 6   // PWM

// ----------------------------
// SENSORES IR
// ----------------------------
const int sensorEsquerdo = A3;
const int sensorCentro  = A0;
const int sensorDireito = A1;

// ----------------------------
// ULTRASSÔNICO
// ----------------------------
#define trigPin 2
#define echoPin A2
const float som = 0.00034029;
float tempoSinal;

bool obstaculo = false;  
bool rfidModeActive = false;

// ----------------------------
// RFID + LCD + PILHA
// ----------------------------
#define SS_PIN 10
#define RST_PIN 7
MFRC522 mfrc522(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// PILHA
const int MAX_PILHA = 5;
int pilha[MAX_PILHA];
int topo = -1;

// ----------------------------
// VARIÁVEIS GERAIS
// ----------------------------
int Sensor1, Sensor2, Sensor3;
int velocidadeBase = 200;


// ==========================================================
// SETUP
// ==========================================================
void setup() {
  Serial.begin(9600);

  // Motores
  pinMode(IN1_ESQUERDO, OUTPUT);
  pinMode(IN2_ESQUERDO, OUTPUT);
  pinMode(PWM_ESQ, OUTPUT);

  pinMode(IN3_DIREITO, OUTPUT);
  pinMode(IN4_DIREITO, OUTPUT);
  pinMode(PWM_DIR, OUTPUT);

  // Sensores
  pinMode(sensorEsquerdo, INPUT);
  pinMode(sensorCentro, INPUT);
  pinMode(sensorDireito, INPUT);

  // Ultrassônico
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // RFID + LCD
  SPI.begin();
  mfrc522.PCD_Init();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  pararCarrinho();
  Serial.println("Sistema iniciado");
}


// ==========================================================
// PILHA
// ==========================================================
void empilhar(int valor) {
  if (topo < MAX_PILHA - 1) {
    topo++;
    pilha[topo] = valor;
  }
}

void desempilhar() {
  if (topo >= 0) topo--;
}

void desempilharTudo() {
  while (topo >= 0) {
    desempilhar();
    mostrarPilhaLCD();
    delay(400);
  }
}

String pilhaToString() {
  String s = "";
  for (int i = 0; i <= topo; i++) {
    s += pilha[i];
    if (i < topo) s += ",";
  }
  return s;
}

void mostrarPilhaLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pilha:");

  lcd.setCursor(0, 1);
  if (topo < 0) {
    lcd.print("vazia");
  } else {
    String txt = pilhaToString();
    if (txt.length() > 16) txt = txt.substring(0, 16);
    lcd.print(txt);
  }
}


// ==========================================================
// ULTRASSÔNICO
// ==========================================================
void gatilhoSensor() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
}

float calculaDistancia(float t) {
  return (t * som) / 2;
}


// ==========================================================
// MOTORES
// ==========================================================
void motorEsquerdoFrente(int v) {
  digitalWrite(IN1_ESQUERDO, HIGH);
  digitalWrite(IN2_ESQUERDO, LOW);
  analogWrite(PWM_ESQ, v);
}

void motorDireitoFrente(int v) {
  digitalWrite(IN3_DIREITO, HIGH);
  digitalWrite(IN4_DIREITO, LOW);
  analogWrite(PWM_DIR, v);
}

void motorEsquerdoParar() {
  digitalWrite(IN1_ESQUERDO, LOW);
  digitalWrite(IN2_ESQUERDO, LOW);
  analogWrite(PWM_ESQ, 0);
}

void motorDireitoParar() {
  digitalWrite(IN3_DIREITO, LOW);
  digitalWrite(IN4_DIREITO, LOW);
  analogWrite(PWM_DIR, 0);
}

void pararCarrinho() {
  motorEsquerdoParar();
  motorDireitoParar();
}


// ==========================================================
// RFID MODE
// ==========================================================
void handleRFIDMode() {

  if (!rfidModeActive) {
    rfidModeActive = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Aproxime o");
    lcd.setCursor(0, 1);
    lcd.print("cartao...");
  }

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Cartão lido
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cartao lido!");
  delay(600);

  int novoValor = topo + 2;
  empilhar(novoValor);

  mostrarPilhaLCD();
  delay(700);

  if (topo == MAX_PILHA - 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pilha cheia!");
    lcd.setCursor(0, 1);
    lcd.print("LIMPANDO...");
    delay(1200);

    desempilharTudo();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pilha vazia!");
    delay(1000);
  }

  mfrc522.PICC_HaltA();
  delay(200);
}


// ==========================================================
// LOOP PRINCIPAL
// ==========================================================
void loop() {

  // --- ULTRASSÔNICO ---
  gatilhoSensor();
  tempoSinal = pulseIn(echoPin, HIGH, 25000);
  float distancia = calculaDistancia(tempoSinal);
  float distCM = distancia * 100;

  // ---------- HYSTERESIS ----------
  static bool travado = false;

  if (!travado && distCM > 0 && distCM < 15) travado = true;
  if (travado && distCM > 20) travado = false;

  obstaculo = travado;

  // ---------- BLOQUEIO + RFID ----------
  if (obstaculo) {
    pararCarrinho();
    handleRFIDMode();
    delay(120);
    return;
  }

  // Saiu do RFID
  if (rfidModeActive) {
    rfidModeActive = false;
    lcd.clear();
    delay(80);
  }

  // ---------- SEGUIDOR DE LINHA ----------
  Sensor1 = digitalRead(sensorEsquerdo);
  Sensor2 = digitalRead(sensorCentro);
  Sensor3 = digitalRead(sensorDireito);

  seguirLinha();

  delay(40);
}



// ==========================================================
// SEGUIDOR DE LINHA
// ==========================================================
void seguirLinha() {
  if (Sensor1 == 1 && Sensor2 == 1 && Sensor3 == 1) {
    pararCarrinho();
    return;
  }

  if (Sensor1 == 1 && Sensor2 == 1 && Sensor3 == 0) {
    motorEsquerdoFrente(150);
    motorDireitoTras(90);
    return;
  }

  if (Sensor1 == 1 && Sensor2 == 0 && Sensor3 == 1) {
    motorEsquerdoFrente(velocidadeBase);
    motorDireitoFrente(velocidadeBase);
    return;
  }

  if (Sensor1 == 0 && Sensor2 == 1 && Sensor3 == 1) {
    motorEsquerdoTras(90);
    motorDireitoFrente(150);
    return;
  }

  pararCarrinho();
}

void motorDireitoTras(int vel) {
  digitalWrite(IN3_DIREITO, LOW);
  digitalWrite(IN4_DIREITO, HIGH);
  analogWrite(PWM_DIR, vel);
}

void motorEsquerdoTras(int vel) {
  digitalWrite(IN1_ESQUERDO, LOW);
  digitalWrite(IN2_ESQUERDO, HIGH);
  analogWrite(PWM_ESQ, vel);
}
