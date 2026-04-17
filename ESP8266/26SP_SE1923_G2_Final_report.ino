
#define BLYNK_TEMPLATE_ID "TMPL6p40byq1m"
#define BLYNK_TEMPLATE_NAME "Plant Monitor"
#define BLYNK_AUTH_TOKEN "u0kMcM0bQXnl5FN0hqpJThzAgspMsmGT"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <Servo.h> 
#include <time.h> 
#include <ESP8266HTTPClient.h> 
#include <WiFiClient.h>        

char ssid[] = "Thien";
char pass[] = "22111997";

const char* city = "Ho Chi Minh,VN"; 
const char* apiKey = "4c270ea925a3946c446edcf4533f09fb";
bool isRaining = false; 

const int SOIL_PIN = A0;     
const int DHT_PIN = 2;       // D4
const int PUMP_PIN = 13;     // D7 
const int SERVO_PIN = 5;     // D1 

SoftwareSerial arduinoSerial(14, 12); // RX = D5, TX = D6

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);
BlynkTimer timer;
Servo roofServo; 

int ROOF_OPEN_ANGLE = 0;     
int ROOF_CLOSE_ANGLE = 90;   



int waterLevel = 1000; 
int lightLevel = 0;          


int lightThreshold = 80;     
int darkThreshold = 30;      
float hotThreshold = 40.0;   
int dryThreshold = 600;      

bool isPumpOn = false;
bool isTankEmpty = false;
bool isRoofClosed = false; 
bool isGrowLightOn = false;  

int isAutoMode = 0;              
unsigned long pumpStartTime = 0; 
bool isAutoWatering = false;     
unsigned long lastWateringTime = 0; 

const unsigned long RUN_TIME = 3000;       
const unsigned long COOLDOWN_TIME = 5000;  

void setup() {
  Serial.begin(9600);
  arduinoSerial.begin(9600);
  
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); 
  
  roofServo.attach(SERVO_PIN, 500, 2400);
  roofServo.write(ROOF_OPEN_ANGLE); 
  
  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Blynk.virtualWrite(V6, 0); 
  Blynk.virtualWrite(V1, 0); 
  Blynk.virtualWrite(V7, 0); 

  timer.setInterval(100L, checkPumpTimer);  
  timer.setInterval(200L, readArduinoData); 
  timer.setInterval(2500L, updateSystem);
  timer.setInterval(900000L, checkWeather); 
  timer.setTimeout(5000L, checkWeather);    
}

void loop() {
  Blynk.run();
  timer.run();
}

void checkWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey);
    
    http.begin(client, url);
    int httpCode = http.GET(); 
    
    if (httpCode > 0) {
      String payload = http.getString(); 
      if (payload.indexOf("\"main\":\"Rain\"") > 0 || payload.indexOf("\"main\":\"Drizzle\"") > 0 || payload.indexOf("\"main\":\"Thunderstorm\"") > 0) {
        isRaining = true;
        Blynk.virtualWrite(V10, "Raining!"); 
      } else {
        isRaining = false;
        Blynk.virtualWrite(V10, "Clear");
      }
    }
    http.end();
  }
}

void readArduinoData() {
  if (arduinoSerial.available() > 0) {
    String msg = arduinoSerial.readStringUntil('\n');
    msg.trim(); 
    
    if (msg.startsWith("W:")) {
      int commaIndex = msg.indexOf(",L:");
      if (commaIndex > 0) {
        waterLevel = msg.substring(2, commaIndex).toInt();
        lightLevel = msg.substring(commaIndex + 3).toInt();
      }
      
      if (waterLevel < 100) {
        isTankEmpty = true;
        digitalWrite(PUMP_PIN, HIGH); 
        isPumpOn = false;
        isAutoWatering = false;
        Blynk.virtualWrite(V1, 0);    
      } else {
        isTankEmpty = false;
      }
    }
  }
}

void checkPumpTimer() {
  if (isAutoWatering) {
    if (millis() - pumpStartTime >= RUN_TIME) {
      digitalWrite(PUMP_PIN, HIGH); 
      isPumpOn = false;
      isAutoWatering = false;
      lastWateringTime = millis();  
      Blynk.virtualWrite(V1, 0);    
    }
  }
}

void updateSystem() {
  int soilValue = analogRead(SOIL_PIN);
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t)) t = 0; 
  if (isnan(h)) h = 0;

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int currentHour = timeinfo->tm_hour; 

  bool isSafeTime = (currentHour < 10 || currentHour >= 16); 
  bool isHardwareReady = (!isTankEmpty && !isPumpOn && !isAutoWatering);
  bool isCooldownFinished = (millis() - lastWateringTime > COOLDOWN_TIME);
  bool isSoilDry = (soilValue > dryThreshold);
  bool isEnvironmentSafe = (t < hotThreshold && isSafeTime && !isRaining);

  if (isAutoMode == 1) {
    if (t > hotThreshold || lightLevel > lightThreshold) {
      if (!isRoofClosed) { 
        roofServo.write(ROOF_CLOSE_ANGLE); 
        isRoofClosed = true; 
        Blynk.virtualWrite(V7, 1); 
      }
    } else {
      if (isRoofClosed) { 
        roofServo.write(ROOF_OPEN_ANGLE); 
        isRoofClosed = false; 
        Blynk.virtualWrite(V7, 0); 
      }
    }

   
    isGrowLightOn = (lightLevel < darkThreshold);

    // water pump
    if (isHardwareReady && isCooldownFinished) { 
      if (isSoilDry && isEnvironmentSafe) {
        digitalWrite(PUMP_PIN, LOW);  
        isPumpOn = true;
        isAutoWatering = true;
        pumpStartTime = millis();     
        Blynk.virtualWrite(V1, 1);    
      }
    }
    
  } else {
    isGrowLightOn = false; 
  }

  Blynk.virtualWrite(V0, soilValue); 
  Blynk.virtualWrite(V2, t);         
  Blynk.virtualWrite(V3, h);         
  Blynk.virtualWrite(V4, waterLevel); 
  Blynk.virtualWrite(V8, lightLevel); 

  arduinoSerial.print("S:");
  arduinoSerial.print(soilValue);
  arduinoSerial.print(",T:");
  arduinoSerial.print((int)t);
  arduinoSerial.print(",P:");
  arduinoSerial.print(isPumpOn ? 1 : 0);
  arduinoSerial.print(",L:"); 
  arduinoSerial.println(isGrowLightOn ? 1 : 0);
}

BLYNK_WRITE(V6) {
  isAutoMode = param.asInt(); 
  if (isAutoMode == 0 && isAutoWatering) {
    digitalWrite(PUMP_PIN, HIGH);
    isPumpOn = false;
    isAutoWatering = false;
    Blynk.virtualWrite(V1, 0);
  }
}

// close/open roof
BLYNK_WRITE(V7) {
  int buttonState = param.asInt();
  
  if (isAutoMode == 1) { 
    Blynk.virtualWrite(V7, isRoofClosed ? 1 : 0); 
    return; 
  }
  
  if (buttonState == 1) { 
    roofServo.write(ROOF_CLOSE_ANGLE); 
    isRoofClosed = true; 
  } else { 
    roofServo.write(ROOF_OPEN_ANGLE); 
    isRoofClosed = false; 
  } 
}

BLYNK_WRITE(V1) {
  int buttonState = param.asInt(); 
  if (isAutoMode == 1) {
    if (isPumpOn == true) {
      Blynk.virtualWrite(V1, 1);
    } else {
      Blynk.virtualWrite(V1, 0);
    }
    return; 
  }
  
  if (buttonState == 1 && !isTankEmpty) {
    digitalWrite(PUMP_PIN, LOW);  
    isPumpOn = true;
    isAutoWatering = false; 
  } else {
    digitalWrite(PUMP_PIN, HIGH); 
    isPumpOn = false;
    isAutoWatering = false;
    if (isTankEmpty) Blynk.virtualWrite(V1, 0); 
  }
  updateSystem(); 
}
