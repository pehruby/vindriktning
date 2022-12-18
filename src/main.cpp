#include "pm1006.h"
#include "secrets.h"
#include <Adafruit_NeoPixel.h>
#include <SensirionI2CScd4x.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <WiFi.h>

#define PIN_FAN 12
#define PIN_LED 25
#define RXD2 16
#define TXD2 17

#define BRIGHTNESS 50
#define BR_NIGHT 5

#define PM_LED 1
#define TEMP_LED 2
#define CO2_LED 3

#define PIN_AMBIENT_LIGHT 4
#define DN 3700 // night level

#define LOOP_MS 120000
#define TEMP_OFFSET 3.02

void setColorWS(byte r, byte g, byte b, int id);
void alert(int id);
void lights_off(void);
void lights_on(void);
void callback(char* topic, byte* message, unsigned int length);
void reconnect();

char scd41_serial_str[20];

uint16_t co2;
float temperature;
float humidity;
uint16_t pm2_5;
uint brightness = 50;

boolean lights = true;
long lastMsg = 0;

WiFiClient espClient;
PubSubClient client(mqtt_server ,1883, callback, espClient);

static PM1006 pm1006(&Serial2);
Adafruit_NeoPixel rgbWS = Adafruit_NeoPixel(3, PIN_LED, NEO_GRB + NEO_KHZ800);
SensirionI2CScd4x scd4x;

void setColorWS(byte r, byte g, byte b, int id) {  // r = hodnota cervene, g = hodnota zelene, b = hodnota modre, id = cislo LED v poradi, kterou budeme nastavovat(1 = 1. LED, 2 = 2. LED atd.)
  uint32_t rgb;  
  rgb = rgbWS.Color(r, g, b); // Konverze vstupnich hodnot R, G, B do pomocne promenne  
  rgbWS.setPixelColor(id - 1, rgb); // Nastavi pozadovanou barvu pro konkretni led = pozice LED zacinaji od nuly
  rgbWS.show();  // Zaktualizuje barvu
}

void alert(int id){
  int i = 0;
  while (1){
     if (i > 10){
      Serial.println("Maybe need Reboot...");
      ESP.restart();
      break;
     }
     rgbWS.setBrightness(255);
     setColorWS(255, 0, 0, id); 
     delay(200);
     rgbWS.setBrightness(brightness);
     setColorWS(0, 0, 0, id);
     delay(200);
     i++;
  }
}


void lights_off(void) {
  setColorWS(0, 0, 0, PM_LED);
  setColorWS(0, 0, 0, TEMP_LED);
  setColorWS(0, 0, 0, CO2_LED);
}

void lights_on(void) {
    
// CO2 LED - horni
  if(co2 < 1000){
    setColorWS(0, 255, 0, CO2_LED);
  }
  
  //svetle zelena
  if((co2 >= 1000) && (co2 < 1200)){
    setColorWS(128, 255, 0, CO2_LED);
  }
  
  // zluta
  if((co2 >= 1200) && (co2 < 1500)){
  setColorWS(255, 255, 0, CO2_LED);
  }
  
  //oranzova
  if((co2 >= 1500) && (co2 < 2000)){
    setColorWS(255, 128, 0, CO2_LED);
  }
  
  if(co2 >= 2000){
    setColorWS(255, 0, 0, CO2_LED);
  }

  // Temperature LED
  if(temperature < 19.0){
    setColorWS(0, 0, 255, TEMP_LED);
  }

  if((temperature >= 19.0) && (temperature < 21.0)){
    setColorWS(0, 255, 0, TEMP_LED);
  }

  if(temperature >= 21.0){
    setColorWS(255, 0, 0, TEMP_LED);
  }
  

  // PM LED - spodni
  if(pm2_5 < 30){
    setColorWS(0, 255, 0, PM_LED);
  }
  
  // svetle zelena
  if((pm2_5 >= 30) && (pm2_5 < 40)){
    setColorWS(128, 255, 0, PM_LED);
  }
  
  // zluta
  if((pm2_5 >= 40) && (pm2_5 < 80)){
  setColorWS(255, 255, 0, PM_LED);
  }
  
  // oranzova
  if((pm2_5 >= 80) && (pm2_5 < 90)){
    setColorWS(255, 128, 0, PM_LED);
  }
  
  if(pm2_5 >= 90){
    setColorWS(255, 0, 0, PM_LED);
  }
  
}

// Process received MQTT message
void callback(char* topic, byte* message, unsigned int length) {
  char mqtt_topic[40];
  char json_value[150];
  String messageLight;
  char messagestr[20];

  int tmpbrightness=0;

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageLight += (char)message[i];
    messagestr[i] = (char)message[i];
  }
  messagestr[length] = '\0';
  Serial.println();
  Serial.print("Length: ");
  Serial.println(length);

  
  // MQTT message for vindriktning/0xhhhhhhhhhhhh/lights/set ?
  sprintf(mqtt_topic,"vindriktning/%s/lights/set", scd41_serial_str);
  if (String(topic) == mqtt_topic) {
    Serial.print("Changing output to ");
    if(messageLight == "on"){
      Serial.println("on");
      lights = true;
      lights_on();
      // digitalWrite(ledPin, HIGH);
    }
    else if(messageLight == "off"){
      lights = false;
      Serial.println("off");
      lights_off();
      // digitalWrite(ledPin, LOW);
    }
    sprintf(mqtt_topic,"vindriktning/%s/lights", scd41_serial_str);
    sprintf(json_value,"{\"switch\": \"%s\"}", messageLight);
    client.publish(mqtt_topic, json_value);
  }
  sprintf(mqtt_topic,"vindriktning/%s/brightness/set", scd41_serial_str);
  if (String(topic) == mqtt_topic) {
    
    tmpbrightness=atoi(messagestr);
    if (tmpbrightness != 0) {
      brightness = tmpbrightness;
      rgbWS.setBrightness(brightness);
      Serial.print("Brightness set to ");
      Serial.println(brightness);
      if (lights) {
        lights_on();
      }
    }
  }
}


void reconnect() {

  char mqtt_topic[50];

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Vindriktning")) {
      Serial.println("connected");
      // Subscribe
      sprintf(mqtt_topic,"vindriktning/%s/lights/set", scd41_serial_str);
      client.subscribe(mqtt_topic);
      sprintf(mqtt_topic,"vindriktning/%s/brightness/set", scd41_serial_str);
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_FAN, OUTPUT); // Fan
  pinMode(PIN_AMBIENT_LIGHT, INPUT); // Ambient light

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  
  rgbWS.begin(); // WS2718
  rgbWS.setBrightness(brightness);
  
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Wire.begin();
  uint16_t error;
  char errorMessage[256];
  scd4x.begin(Wire);
  
  Serial.println("Start...");
  delay(500);
  Serial.println("1. LED Green");
  setColorWS(0, 255, 0, PM_LED);
  delay(1000);
  Serial.println("2. LED Green");
  setColorWS(0, 255, 0, TEMP_LED);
  delay(1000);
  Serial.println("3. LED Green");
  setColorWS(0, 255, 0, CO2_LED);
  delay(1000);
  setColorWS(0, 0, 0, PM_LED);
  setColorWS(0, 0, 0, TEMP_LED);
  setColorWS(0, 0, 0, CO2_LED);

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  }

  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
      Serial.print("SCD41 Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  } else {
      sprintf(scd41_serial_str,"0x%04x%04x%04x", serial0, serial1, serial2);
      Serial.print("SCD41 Serial: ");
      Serial.println(scd41_serial_str);
  }
  
  /*------------- Wi-Fi -----------*/
  WiFi.begin(ssid, password);
  Serial.println("Pripojovani");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Pripojeno do site, IP adresa zarizeni: ");
  Serial.println(WiFi.localIP());

  //client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  }

}

void loop() {
  uint16_t error;
  int al;

  char errorMessage[256];
  char tempString[8];
  char humiString[8];
  char co2String[8];
  char pm2_5String[8];
  char json_value[150];
  char mqtt_topic[40];
  char messageLight[5];


  if (!client.connected()) {
      // connect to MQTT server, subscribe to topic
      reconnect();
  }
  // check if MQTT message was received
  client.loop();
  long now = millis();
  // perform meassurement each LOOP_MS miliseconds
  if (lastMsg == 0 or (now - lastMsg > LOOP_MS)) {
    lastMsg = now;
    
    // start fan for 10sec
    digitalWrite(PIN_FAN, HIGH);
    Serial.println("Fan ON");
    delay(10000);

    if (pm1006.read_pm25(&pm2_5)) {
      printf("PM2.5 = %u\n", pm2_5);
    } else {
      Serial.println("Measurement failed!");
      alert(PM_LED);
    }

    delay(1000);
    digitalWrite(PIN_FAN, LOW);
    Serial.println("Fan OFF");
    

    // read CO2, temperature and humidity from SCD
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      Serial.print("SCD41 Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
    } else if (co2 == 0) {
      Serial.println("Invalid sample detected, skipping.");
    } else {
      temperature = temperature - TEMP_OFFSET;
      
      Serial.print("Co2:");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print(" Temperature:");
      Serial.print(temperature);
      Serial.print("\t");
      Serial.print(" Humidity:");
      Serial.println(humidity);


      // convert values to strings
      dtostrf(temperature, 1, 2, tempString);
      dtostrf(humidity, 1, 2, humiString);
      dtostrf(co2, 1, 0, co2String);
      dtostrf(pm2_5, 1, 0, pm2_5String);
      sprintf(mqtt_topic,"vindriktning/%s", scd41_serial_str);
      sprintf(json_value,"{\"temperature\": %s, \"humidity\": %s, \"co2\": %s, \"pm25\": %s}", tempString, humiString, co2String, pm2_5String);

      Serial.print("MQTT: ");
      Serial.print(mqtt_topic);
      Serial.print(" ");
      Serial.println(json_value);
      // publish values to MQTT server
      client.publish(mqtt_topic, json_value);

      if (lights) {
        lights_on();
      }
    }
  }
}

