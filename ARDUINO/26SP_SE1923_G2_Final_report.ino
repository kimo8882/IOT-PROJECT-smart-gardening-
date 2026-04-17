
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

SoftwareSerial espSerial(8, 9); 
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const int LIGHT_SENSOR_PIN = A0; 
const int WATER_SENSOR_PIN = A1; 
const int GROW_LIGHT_PIN = 7;     

unsigned long lastSendTime = 0;

int soilValue = 0;
int tempValue = 0;
int pumpStatus = 0;

void setup() {
  Serial.begin(9600);      
  espSerial.begin(9600);   
  
  pinMode(GROW_LIGHT_PIN, OUTPUT);
  digitalWrite(GROW_LIGHT_PIN, LOW); 
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting..");
  delay(2000);
  lcd.clear();
}

void loop() {
  if (espSerial.available() > 0) {
    String msg = espSerial.readStringUntil('\n');
    msg.trim(); 
    
    if (msg.startsWith("S:")) {
      int comma1 = msg.indexOf(',');
      int comma2 = msg.indexOf(',', comma1 + 1);
      int comma3 = msg.indexOf(',', comma2 + 1); 
      
      if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
        soilValue = msg.substring(2, comma1).toInt();
        tempValue = msg.substring(comma1 + 3, comma2).toInt();
        pumpStatus = msg.substring(comma2 + 3, comma3).toInt();
        int ledStatus = msg.substring(comma3 + 3).toInt(); 

        if (ledStatus == 1) {
          digitalWrite(GROW_LIGHT_PIN, HIGH);
        } else {
          digitalWrite(GROW_LIGHT_PIN, LOW);
        }
        
        updateLCD(); 
      }
    }
  }

  // send data
  if (millis() - lastSendTime > 2000) {
    int waterLevel = analogRead(WATER_SENSOR_PIN);
    int lightLevel = analogRead(LIGHT_SENSOR_PIN); 
    
    espSerial.print("W:");
    espSerial.print(waterLevel);
    espSerial.print(",L:");
    espSerial.println(lightLevel);
    
    updateLCD(); 
    lastSendTime = millis();
  }
}

void updateLCD() {
  int currentWater = analogRead(WATER_SENSOR_PIN);
  int currentLight = analogRead(LIGHT_SENSOR_PIN);
  

  lcd.setCursor(0, 0);
  lcd.print("                "); 
  lcd.setCursor(0, 0);
  lcd.print("S:");
  lcd.print(soilValue);
  lcd.print(" T:");
  lcd.print(tempValue);
  lcd.print(" L:");
  lcd.print(currentLight);


  lcd.setCursor(0, 1);
  lcd.print("                "); 
  lcd.setCursor(0, 1);
  
  if (currentWater < 100) {
    lcd.print("ERR: TANK EMPTY!");
  } else {
    lcd.print("W:");
    lcd.print(currentWater);
    if (pumpStatus == 1) {
      lcd.print(" PMP:ON");
    } else {
      lcd.print(" PMP:OFF");
    }
  }
}
