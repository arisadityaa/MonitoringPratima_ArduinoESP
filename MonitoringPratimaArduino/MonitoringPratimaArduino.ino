#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>

// define pin
#define pinDHT 3 //pin DHT
DHT dht(pinDHT, DHT22); //DHT Setup
#define microwaveSensor 6
#define flameSensor A0
#define TrigPin 4
#define EchoPin 5

// deklarasi Variabel
int motion, flame, distance;
float kelembaban, suhu;
long duration;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(microwaveSensor, INPUT);
  pinMode(flameSensor, INPUT);
  pinMode(TrigPin, OUTPUT);
  pinMode(EchoPin, INPUT);
  dht.begin();

}

void loop() {
  // put your main code here, to run repeatedly:
  String kirim;
  motion = digitalRead(microwaveSensor);
  kelembaban = dht.readHumidity();
  suhu = dht.readTemperature();
  flame = analogRead(flameSensor);
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  duration = pulseIn(EchoPin, HIGH);
  distance = duration * (0.034 / 2);
  kirim = String(distance) + "#" + String(suhu) + "#" + String(kelembaban)+ "#" + String(flame) + "#" + String(motion)+";";
  Serial.println(kirim);
  delay(500);
}
