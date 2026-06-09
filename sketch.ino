#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

const uint8_t PIN_BTN[5] = {2, 3, 4, 5, 6};
const uint8_t PIN_PIR = 7;
const uint8_t PIN_SR_DAT = 8;
const uint8_t PIN_SERVO = 9;
const uint8_t PIN_SR_LAT = 10;
const uint8_t PIN_SR_CLK = 11;
const uint8_t PIN_LED_HOT = 12;
const uint8_t PIN_LED_COLD = 13;
const uint8_t PIN_LDR = A0;
const uint8_t PIN_DHT = A1;
const uint8_t PIN_IR = A2;
const uint8_t PIN_BUZZER = 17;
float tempSetpoint = 25.0;
const float TEMP_ZM = 3.0;

uint8_t luxSetpoint = 80;
const uint8_t LUX_ZM = 5;

const uint8_t ANG_PLANTA[5] = {0, 45, 90, 135, 180};

const uint16_t T_CTRL_MS = 500;
const uint16_t T_LCD_MS = 1000;
const uint16_t T_SERVO_MS = 30;

Servo cabina;
DHT dht(PIN_DHT, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t plantaActual = 0;
volatile uint8_t plantaDestino = 0;

int anguloActualServo = 0;
int anguloDestinoServo = 0;

float tempActual = 25.0;
float tempAnterior = 25.0;

uint8_t luxActual = 80;
uint8_t luxAnterior = 80;

uint8_t ledsEncendidos = 4;
bool presencia = false;

bool falloDHT = false;
bool falloLDR = false;
bool falloPIR = false;
bool falloServo = false;

volatile bool llamadaInterrupcion = false;
volatile unsigned long ultimaInterrupcion = 0;

unsigned long tCtrl = 0;
unsigned long tLcd = 0;
unsigned long tServo = 0;

void setup() {
Serial.begin(9600);

for (uint8_t i = 0; i < 5; i++) {
pinMode(PIN_BTN[i], INPUT_PULLUP);
}

//tone(PIN_BUZZER,1000);
//delay(1000);
//noTone(PIN_BUZZER);

pinMode(PIN_PIR, INPUT);
pinMode(PIN_SR_DAT, OUTPUT);
pinMode(PIN_SR_LAT, OUTPUT);
pinMode(PIN_SR_CLK, OUTPUT);
pinMode(PIN_LED_HOT, OUTPUT);
pinMode(PIN_LED_COLD, OUTPUT);
pinMode(PIN_BUZZER, OUTPUT);
digitalWrite(PIN_BUZZER, LOW);
cabina.attach(PIN_SERVO);
cabina.write(ANG_PLANTA[0]);

dht.begin();

IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);

attachInterrupt(digitalPinToInterrupt(PIN_BTN[0]), interrupcionPlanta0, FALLING);
attachInterrupt(digitalPinToInterrupt(PIN_BTN[1]), interrupcionPlanta1, FALLING);

lcd.init();
lcd.backlight();
lcd.print("ACME Ascensor");
delay(1500);
lcd.clear();

actualizarLeds(ledsEncendidos);

Serial.println("Sistema listo");
Serial.print("SP Temp inicial: ");
Serial.print(tempSetpoint);
Serial.print(" C | SP Luz inicial: ");
Serial.print(luxSetpoint);
Serial.println(" %");
}

void loop() {
leerControlRemoto();
leerPulsadores();
moverCabina();

if (millis() - tCtrl >= T_CTRL_MS) {
tCtrl = millis();
leerSensores();
autodiagnostico();
controlTemperatura();
controlIluminacion();
alarma();
}

if (millis() - tLcd >= T_LCD_MS) {
tLcd = millis();
mostrarLCD();
}
}

void interrupcionPlanta0() {
unsigned long ahora = millis();

if (ahora - ultimaInterrupcion > 200) {
plantaDestino = 0;
llamadaInterrupcion = true;
ultimaInterrupcion = ahora;
}
}

void interrupcionPlanta1() {
unsigned long ahora = millis();

if (ahora - ultimaInterrupcion > 200) {
plantaDestino = 1;
llamadaInterrupcion = true;
ultimaInterrupcion = ahora;
}
}

void leerPulsadores() {
if (llamadaInterrupcion) {
llamadaInterrupcion = false;
Serial.print("Llamada por interrupcion a planta ");
Serial.println(plantaDestino);
}

for (uint8_t i = 2; i < 5; i++) {
if (digitalRead(PIN_BTN[i]) == LOW) {
delay(30);
if (digitalRead(PIN_BTN[i]) == LOW) {
plantaDestino = i;
Serial.print("Llamada planta ");
Serial.println(i);
delay(200);
}
}
}
}

void leerControlRemoto() {
if (IrReceiver.decode()) {
uint8_t comando = IrReceiver.decodedIRData.command;

Serial.print("Codigo IR: ");
Serial.println(comando, HEX);

switch (comando) {
case 0x02:
tempSetpoint++;
Serial.print("Setpoint temperatura aumentado: ");
Serial.print(tempSetpoint);
Serial.println(" C");
break;

case 0x98:
tempSetpoint--;
Serial.print("Setpoint temperatura reducido: ");
Serial.print(tempSetpoint);
Serial.println(" C");
break;

case 0x90:
if (luxSetpoint <= 95) luxSetpoint += 5;
Serial.print("Setpoint luz aumentado: ");
Serial.print(luxSetpoint);
Serial.println(" %");
break;

case 0xE0:
if (luxSetpoint >= 5) luxSetpoint -= 5;
Serial.print("Setpoint luz reducido: ");
Serial.print(luxSetpoint);
Serial.println(" %");
break;

case 0x68:
tempSetpoint = 25.0;
luxSetpoint = 80;
Serial.println("Setpoints reiniciados");
break;

case 0x30:
plantaDestino = 0;
Serial.println("IR: planta 0");
break;

case 0x18:
plantaDestino = 1;
Serial.println("IR: planta 1");
break;

case 0x7A:
plantaDestino = 2;
Serial.println("IR: planta 2");
break;

case 0x10:
plantaDestino = 3;
Serial.println("IR: planta 3");
break;

case 0x38:
plantaDestino = 4;
Serial.println("IR: planta 4");
break;

default:
Serial.println("Comando IR no asignado");
break;
}

Serial.print("SP Temp actual: ");
Serial.print(tempSetpoint);
Serial.print(" C | SP Luz actual: ");
Serial.print(luxSetpoint);
Serial.println(" %");

IrReceiver.resume();
}
}

void moverCabina() {
anguloDestinoServo = ANG_PLANTA[plantaDestino];

if (anguloActualServo == anguloDestinoServo) {
plantaActual = plantaDestino;
return;
}

if (millis() - tServo >= T_SERVO_MS) {
tServo = millis();

if (anguloActualServo < anguloDestinoServo) {
anguloActualServo++;
} else if (anguloActualServo > anguloDestinoServo) {
anguloActualServo--;
}

cabina.write(anguloActualServo);

if (anguloActualServo == anguloDestinoServo) {
plantaActual = plantaDestino;
falloServo = false;

Serial.print("Cabina en planta ");
Serial.println(plantaActual);
}
}
}

void leerSensores() {
tempAnterior = tempActual;

float t = dht.readTemperature();

if (!isnan(t)) {
tempActual = t;
}

luxAnterior = luxActual;

int raw = analogRead(PIN_LDR);
luxActual = map(raw, 0, 1023, 0, 100);

presencia = digitalRead(PIN_PIR);
}

void autodiagnostico() {
falloDHT = false;
falloLDR = false;
falloPIR = false;

float t = dht.readTemperature();

if (isnan(t) || t < -20 || t > 80) {
falloDHT = true;
tempActual = tempAnterior;
Serial.println("ERROR DHT22");
}

int rawLdr = analogRead(PIN_LDR);

if (rawLdr <= 20 || rawLdr >= 1000) {
falloLDR = true;
luxActual = luxAnterior;
Serial.println("ERROR LDR");
}

if (digitalRead(PIN_PIR) != HIGH && digitalRead(PIN_PIR) != LOW) {
falloPIR = true;
Serial.println("ERROR PIR");
}

if (falloDHT || falloLDR || falloPIR || falloServo) {
Serial.println("MODO SEGURO ACTIVADO");
}
}

void controlTemperatura() {
if (falloDHT) {
digitalWrite(PIN_LED_HOT, LOW);
digitalWrite(PIN_LED_COLD, LOW);
return;
}

float limMax = tempSetpoint + TEMP_ZM;
float limMin = tempSetpoint - TEMP_ZM;

if (tempActual > limMax) {
digitalWrite(PIN_LED_COLD, HIGH);
digitalWrite(PIN_LED_HOT, LOW);
} else if (tempActual < limMin) {
digitalWrite(PIN_LED_HOT, HIGH);
digitalWrite(PIN_LED_COLD, LOW);
} else {
digitalWrite(PIN_LED_HOT, LOW);
digitalWrite(PIN_LED_COLD, LOW);
}
}

void controlIluminacion() {
if (falloLDR) {
actualizarLeds(4);
return;
}

if (luxActual < luxSetpoint - LUX_ZM && ledsEncendidos < 8) {
ledsEncendidos++;
} else if (luxActual > luxSetpoint + LUX_ZM && ledsEncendidos > 0) {
ledsEncendidos--;
}

actualizarLeds(ledsEncendidos);
}

void actualizarLeds(uint8_t n) {
uint8_t patron = (n >= 8) ? 0xFF : ((1 << n) - 1);

digitalWrite(PIN_SR_LAT, LOW);
shiftOut(PIN_SR_DAT, PIN_SR_CLK, MSBFIRST, patron);
digitalWrite(PIN_SR_LAT, HIGH);
}

void mostrarLCD() {
lcd.clear();

lcd.setCursor(0, 0);
lcd.print("P:");
lcd.print(plantaActual);
lcd.print(" T:");
lcd.print(tempActual, 0);
lcd.print("/");
lcd.print(tempSetpoint, 0);

lcd.setCursor(0, 1);

if (falloDHT || falloLDR || falloPIR || falloServo) {
lcd.print("ERROR SENSOR");
} else {
lcd.print("L:");
lcd.print(luxActual);
lcd.print("/");
lcd.print(luxSetpoint);
lcd.print(" ");
lcd.print(presencia ? "OCU" : "LIB");
}
}

void alarma(){
  if(falloDHT || falloLDR || falloPIR || falloServo){
    digitalWrite(PIN_BUZZER, HIGH);
  } else{
    digitalWrite(PIN_BUZZER, LOW);
  }
}