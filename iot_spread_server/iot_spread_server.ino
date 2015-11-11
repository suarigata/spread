//#define GALILEO 1

#ifdef GALILEO
  #include <Ethernet.h>
#else
  #include <WiFi.h>
#endif

#include <Wire.h>
#include "rgb_lcd.h"

/* Variables */
  const int myID = 1; // server
  typedef struct {
    int id;
    int timestamp = -1;
    float temperature;
    bool buttonPressed;
  } data;
  data allData[3];
  bool debug = false;
  HardwareSerial& mSerial = (debug == true? Serial : Serial1);

#ifdef GALILEO
  EthernetClient client;
#else
  WiFiClient client;
#endif
  // Variable Setup
  int failedCounter = 0;

  // ThingSpeak Settings
  char tsIP[] = "144.212.80.11";// OR "api.thingspeak.com"
  char tsNS[] = "api.thingspeak.com";
  char *key[3] = {"HT6PH1T63UDXHU2X","M9AO2YMKVX5KJ4YF","1KK2L7AHU0Q1RGJS"};
  char *canal[3]={"64961","65113","65114"};
  int chances = 0;

  //update?key=M9AO2YMKVX5KJ4YF&field2=1&field3=44.587&field1=5
  //channels/65114/fields/2/last
  
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

    if (debug) Serial.println("Starting internet");
    
#ifdef GALILEO
    startEthernet();
#endif
}

int syncTime=100000;
void loop(){
  if(syncTime++>150){ // 75 eh mais ou menos 15s, tempo minimo de aquisicao do thingspeak
    Serial.print("< sync");
    if(syncData())
      syncTime=0;
    
    createData();
    Serial.println(" >");
  }

  delay(200);
    
  if(mSerial.available() > 0) {
    char c = mSerial.peek();
    while(c!='a' && c!=-1) {
      c = mSerial.read();
      c = mSerial.peek();
    }
    if (c=='a') listen();
    delay(100);
    digitalWrite(ledPin,LOW);
  }
}

boolean syncData(){
  char resp[21];
  char bu[50];
  int ts;
  
  WiFi.begin("internet","32960620");
  if(WiFi.status()==WL_CONNECTED)
    Serial.println("\nConected");
  
  if(!client.connected() && !client.connect(tsNS,80)){
    client.stop();
    Serial.println("No web connection");
    return false;
  }

  for(int i=0;i<3;i++){
    sprintf(bu,"/channels/%s/fields/1/last",canal[i]);
    Serial.println(bu);
    getTS(bu,resp);
    Serial.println(resp);
    ts=0;
    if(resp[0]=='"')
      ts=-1;
    else
      for(int j=0;j<20 && resp[j];j++){
        ts=ts*10+resp[j]-'0';
      }
    Serial.println(ts);
    if(allData[i].timestamp>ts){
      sprintf(bu,"/update?key=%s&field1=%i&field2=%i&field3=%.2f",key[i],allData[i].timestamp,allData[i].buttonPressed?1:0,allData[i].temperature);
      Serial.println(bu);
      getTS(bu,resp);
    }
    else if(allData[i].timestamp<ts){
      // Timestamp
      allData[i].timestamp=ts;
      // Botao
      sprintf(bu,"/channels/%s/fields/2/last",canal[i]);
      Serial.println(bu);
      getTS(bu,resp);
      Serial.println(resp);
      allData[i].buttonPressed=resp[0]=='1';
      // Temperatura
      sprintf(bu,"/channels/%s/fields/3/last",canal[i]);
      Serial.println(bu);
      getTS(bu,resp);
      
      allData[i].temperature=0;
      int j=0;
      while(resp[j] && resp[j]!='.')
        allData[i].temperature=10*allData[i].temperature+resp[j++]-'0';
      if(resp[j] =='.')
        allData[i].temperature+=(resp[++j]-'0')/10.0;
      if(resp[j+1] <= '9' && resp[j+1]>='0')
        allData[i].temperature+=(resp[++j]-'0')/100.0;
      Serial.println(allData[i].temperature);
      updateLCD(i);
    }
  }
  return true;
}

void getTS(char *dir,char *resp){
  client.flush();
  client.print("GET ");
  client.print(dir);
  client.println(" HTTP/1.1");
  client.print("HOST: ");
  client.println(tsNS);
  client.println();
  //delay(100);
  int i=0;
  char o;
  while(client.available()){
    o=client.read();
    if(o=='\n')
      if(i)
        break;
      else
        i=1;
    else
      if(o!='\r')
        i=0;
  }
  i=0;
  resp[0]='\0';
  while(client.available()){
    o=client.read();
    if(o=='\n' || o=='\r'){
      client.read();
      break;
    }
    i=i*10+o-'0';
  }

  int j=0;
  while(client.available() && j<i && j<20){
    o=client.read();
    if(o=='\n' || o=='\r')
      continue;
    resp[j++]=o;
  }
  resp[j]='\0';
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
  char c;
  ////// "anyone?"
  char anyone[] = "anyone?";
  for (int i=0; i<7; i++) {
    if (waitForData() == false) return;
    chances = 0;
    c = mSerial.read();
    if (c!=anyone[i]) {
      Serial.print("Connection failed, expected ");
      Serial.print(anyone[i]);
      Serial.print(", got ");
      Serial.println(c);
      while(mSerial.available()>0) c = mSerial.read();
      return;
    }
  }
  
  ////// clientID
  c = mSerial.read();
  int clientID = charToInt(c);
  if (clientID < 0 || clientID > 2) {
    Serial.println("ClientID invalido");
    return;
  }

  ////// > "hello" + clientID + serverID
  Serial1.print("hello");
  Serial1.print(clientID);
  Serial1.print(myID);
  Serial.print("> hello");
  Serial.print(clientID);
  Serial.println(myID);

  ////// < "ack" + serverID
  if (waitForData() == false) return;
  chances = 0;
  char ack[] = "ack";
  for (int i=0; i<3; i++) {
    c = mSerial.read();
    if (c!=ack[i]) {
      Serial.print("Ack failed, expected ");
      Serial.print(ack[i]);
      Serial.print(", got ");
      Serial.println(c);
      return;
    }
  }
  c = mSerial.read();
  Serial.print("Received ID: ");
  Serial.println(charToInt(c));
  if (myID == charToInt(c)) {
    Serial.println("Connected!");
  }

  ////// > quantity
  digitalWrite(ledPin,HIGH);
  int quantity = 3;
  bool sendData[3] = {true, true, true};
  for (int i=0; i<3; i++) {
    if (allData[i].timestamp == -1) {
      sendData[i] = false;
      quantity--;
    }
  }
  Serial1.print(quantity);
  Serial.print("Sent quantity ");
  Serial.println(intToChar(quantity));

  ////// > "<" + id + "," + timestamp + "," + buttonPressed + "," + temperature + ">"
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
  
  ////// < quantity
  if (waitForData() == false) return;
  chances = 0;
  c = mSerial.read();
  quantity = charToInt(c);
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

    if (timestamp > allData[id].timestamp) {
      allData[id].timestamp = timestamp;
      allData[id].temperature = temp;
      allData[id].buttonPressed = button;
      Serial.print("Received update for device #");
      Serial.println(intToChar(id));
      if (button) Serial.println("BUTTON PRESSED");
      else Serial.println("BUTTON NOT PRESSED");
      updateLCD(id);
    }
  }
}

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
  return (char) 48 + num;
}

int charToInt(char c) {
  return (int) c - 48;
}

