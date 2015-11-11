#include <WiFi.h>
#include <Wire.h>
#include "rgb_lcd.h"

/* Variables */
  const int myID = 2; // client
  typedef struct {
    int id;
    int timestamp = -1;
    float temperature;
    bool buttonPressed;
  } data;
  data allData[3];
  bool debug = false;
  HardwareSerial& mSerial = (debug == true? Serial : Serial1);
  WiFiClient client;
  char thingSpeakAddress[] = "144.212.80.11";
  String writeAPIKey = "879ULVPKL7PF0UOB";
  
/* Sensors */
  const int temperaturePin = A1;
  const int buttonPin = 8;
  const int ledPin = 4;
  rgb_lcd lcd;

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);
    Serial.println("Initializing");
    pinMode(buttonPin, INPUT);
    pinMode(ledPin,OUTPUT);
    lcd.begin(16,2);
    lcd.setRGB(0,60,50);
    lcd.clear();
    lcd.print("|0   |1   |2   |");
    lcd.setCursor(0,1);
    lcd.print("|    |    |    |");
    for (int i=0; i<5; i++) {
      digitalWrite(ledPin,HIGH);
      delay(100);
      digitalWrite(ledPin,LOW);
      delay(100);
    }

    for (int i=0; i<3; i++)
    {
      allData[i].id = i;
      allData[i].timestamp = -1;
    }
    
    if (debug) mSerial = Serial;
    else mSerial = Serial1;

    if (debug) Serial.println("Listening to Serial");
    else Serial.println("Listening to Serial1");
}

void loop() {
  createData();
  //Serial.println("Syncing data");
  //syncData();
  delay(2000);
  
  while(mSerial.available() > 0) mSerial.read();
  
  Serial1.print("anyone?");
  Serial1.print(myID);
  Serial.println("> anyone?");
  
  delay(500);
  
  if (mSerial.available() > 0) {
    ////// < "hello" + myID + serverID
    char hello[6] = {'h','e','l','l','o','0'};
    hello[5] = intToChar(myID);
  
    for (int i=0; i<6; i++) {
      char c = mSerial.read();
      if (c!=hello[i]) {
        Serial.println("Connection unsuccessful");
        while (mSerial.available() > 0) {
          c = mSerial.read();
        }
        return;
      }
    }
    Serial.println("Found someone!");
    listen();
    delay(100);
    digitalWrite(ledPin,LOW);
  } //else Serial.println("... no one to find.");
}

void syncData() {
  for (int i=0; i<3; i++) {
    if (allData[i].timestamp == -1) continue;
    char buff[5];
    sprintf(buff, "%.1f", allData[i].temperature);
    String temp = String(buff);
    sendData("field5="+temp+"&field6="+(allData[i].buttonPressed? 1 : 0));
  }
}

void sendData(String data) {
  if (client.connected()) {
    //client.stop();
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(data.length());
    client.print("\n\n");
    client.print(data);
    return;
  }

  if (client.connect(thingSpeakAddress, 80)) {
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(data.length());
    client.print("\n\n");
    client.print(data);
  }
}

void createData() {
    allData[myID].timestamp++;
    allData[myID].temperature = lm35ToCelsius(analogRead(A1));
    allData[myID].buttonPressed = (digitalRead(buttonPin) == HIGH);
    /*Serial.print("Data created: timestamp ");
    Serial.print(timestamp);
    Serial.print(", temperature ");
    Serial.print(allData[myID].temperature);
    Serial.print(", button pressed ");
    Serial.print(allData[myID].buttonPressed);
    Serial.print("\n");*/
    updateLCD(myID);
}

float lm35ToCelsius(int value) {
  return (value * 0.048828125);
}

void listen() {
  while(mSerial.available() > 0) {
    char c;

    ////// serverID
    c = mSerial.read();
    int serverID = charToInt(c);
    if (serverID < 0 || serverID > 2) {
      Serial.println("SerialID invalido");
      return;
    }

    ////// > "ack" + serverID
    digitalWrite(ledPin,HIGH);
    Serial1.print("ack");
    Serial1.print(serverID);
    Serial.print("> ack");
    Serial.println(serverID);

    ////// < quantity
    if (waitForData() == false) return;
    bool sendData[3] = {true, true, true};
    for (int i=0; i<3; i++) {
      if (allData[i].timestamp == -1)
        sendData[i] = false;
    }
    c = mSerial.read();
    int quantity = charToInt(c);
    if (quantity < 0 || quantity > 3) {
      Serial.print("Quantidade invalida: ");
      Serial.println(quantity);
      return;
    }
    Serial.print("Quantidade: ");
    Serial.println(quantity);

    ////// < "<" + id + "," + timestamp + "," + buttonPressed + "," + temperature + ">"
    for (int i=0; i<quantity; i++) {
      ////// "<"
      if (waitForData() == false) return;
      c = mSerial.read();
      if (c != '<') {
        Serial.print("\"<\" expected but got something else: ");
        Serial.println(c);
        return;
      }
      ////// id
      c = mSerial.read(); // id
      int id = charToInt(c);
      ////// ","
      c = mSerial.read();
      if (c != ',') {
        Serial.print("\",\" expected but got something else: ");
        Serial.println(c);
        return;
      }
      ////// timestamp + ","
      int timestamp = 0;
      c = mSerial.read(); // timestamp
      while (c!=',') {
        timestamp *= 10;
        timestamp += charToInt(c);
        c = mSerial.read();
      }
      ////// "P" or "N"
      c = mSerial.read(); // 'P'/'N'
      bool button = (c == 'P'? true : false);
      ////// ","
      c = mSerial.read();
      if (c != ',') {
        Serial.print("\",\" expected but got something else: ");
        Serial.println(c);
        return;
      }
      ////// temperature
      float temp = 0;
      c = mSerial.read(); // 10 t
      temp += 10 * charToInt(c);
      c = mSerial.read(); // 1 t
      temp += charToInt(c);
      c = mSerial.read(); // '.'
      if (c != '.') {
        Serial.print("\".\" expected but got something else: ");
        Serial.println(c);
        return;
      }
      c = mSerial.read(); // 0.1 t
      temp += 0.1 * charToInt(c);
      c = mSerial.read(); // 0.01 t
      temp += 0.01 * charToInt(c);
      ////// ">"
      c = mSerial.read();
      if (c != '>') {
        Serial.print("\">\" expected but got something else: ");
        Serial.println(c);
        return;
      }

      /*Serial.print("Current timestamp: ");
      Serial.println(allData[id].timestamp);
      Serial.print("New timestamp: ");
      Serial.println(timestamp);*/
      if (timestamp > allData[id].timestamp) {
        allData[id].timestamp = timestamp;
        allData[id].temperature = temp;
        allData[id].buttonPressed = button;
        Serial.println(intToChar(id));
        sendData[id] = false;
        updateLCD(id);
      }
    }

    ////// > quantity
    quantity = 0;
    for (int i=0; i<3; i++) {
      if (sendData[i]) quantity++;
    }
    /*if (quantity == 0) Serial1.print("0");
    if (quantity == 1) Serial1.print("1");
    if (quantity == 2) Serial1.print("2");
    if (quantity == 3) Serial1.print("3");*/
    Serial1.print(quantity);
    Serial.print("> quantity ");
    Serial.println(quantity);

    ////// < "<" + id + "," + timestamp + "," + buttonPressed + "," + temperature + ">"
    for (int i=0; i<quantity; i++) {
      int id = 0;
      while(!sendData[id]) id++;
      sendData[id] = false;
      Serial1.print("<");
      Serial1.print(id);
      Serial1.print(",");
      Serial1.print(allData[id].timestamp);
      Serial1.print(",");
      Serial1.print((allData[id].buttonPressed? "P" : "N"));
      Serial1.print(",");
      Serial1.print(allData[id].temperature);
      Serial1.print(">");
    }
  }
}

void updateLCD(int id) {
  lcd.setCursor(4+5*id,0);
  if (allData[id].buttonPressed) lcd.print("P");
  else lcd.print("N");

  lcd.setCursor(1+5*id,1);
  char buff[5];
  sprintf(buff, "%.1f", allData[id].temperature);
  String temp = String(buff);
  lcd.print(temp);
}

char intToChar(int num) {
  return (char) '0' + num;
}

int charToInt(char c) {
  return (int) c - '0';
}

int chances = 0;
bool waitForData() {
  while (mSerial.peek() == -1) {
    delay(10);
    chances++;
    if (chances > 200) {
      chances = 0;
      return false;
    }
  }
  return true;
}

