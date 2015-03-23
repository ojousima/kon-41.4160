#include <AFMotor.h>
#include <Adafruit_ESP8266.h>

AF_Stepper motor(200, 1); //Init stepper motor

//Coffee maker pin definitions
#define BUTTON  31
#define RELAY  33
#define PUMP  35

//ESP Debug, 0 for off, >0 for on
#define WIFI_DEBUG 0

//ESP Pin definitions
#define ESP_RX   15
#define ESP_TX   14
#define ESP_RST  37

#if WIFI_DEBUG
  Adafruit_ESP8266 wifi(&Serial3, &Serial, ESP_RST); //&Serial refers to hardware serial
#else
  Adafruit_ESP8266 wifi(&Serial3, 0, ESP_RST); //Debug data is directed to null
#endif

#define ESP_SSID "aalto open" // Your network name here
#define ESP_PASS "" // Your network password here

#define HOST     "ojousima.net"     // Host to contact
#define PORT     80                 // 80 = HTTP default port

//ESP Constant definitions
#define CHECK_INTERVAL 300000L      //Check every 5 minutes.
#define CHANGE_RATE 1000L

long CUP_CHECK_TIME = 0;
long cups, time;
int notAborted;
const uint8_t buffer_length = 50;
char buffer[buffer_length];
const uint8_t data_length = 10;
char data[data_length];

void setup()
{
  Serial.begin(57600); while(!Serial); // UART serial debug

  wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready")); //WiFi firmware version
  Serial3.begin(9600); // Soft serial connection to ESP8266

  //Pump, relay setup
  motor.setSpeed(10);
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, HIGH);
}

void loop()
{
  Serial.print("Cups of coffee to make: ");
  cups = askNumberOfCups();
  Serial.println(cups);
  if (cups >= 1 && cups <= 10) {
    pressPowerButton();
    doseCoffeeGrounds(cups);
    if (notAborted) {
      fillTank(cups);
    }
    if (notAborted) {
      waitThatCoffeeIsReady();
    }
    if (notAborted) {
      waitForAutoOff();
    }
  }
  else {
    Serial.println("Invalid number");
  }
}

long askNumberOfCups()
{
  long input = 0;
  //Pyöritetään tätä niin kauan, kun lokaalista sarjaportista TAI WiFi:ltä ei tule uutta käskyä.
  while (Serial.available() == 0 && input == 0) {
    // Jos kahvinkeittimen virtanappulaa painaa, kahvinkeitin menee manuaalimoodiin.
    if (digitalRead(BUTTON) == HIGH) {
      manualMode();
    }
    // Tarkistetaan, onko WiFi:llä uusia käskyjä.
    input = requestCupsOverWiFi();
  }
  // Suoritetaan, mikäli lokaalista sarjaportista tuli uusi käsky, mutta WiFiltä ei.
  while (Serial.available() > 0 && input == 0) {
    input *= 10;
    input += Serial.read() - '0';
    delay(10);
  }
  return input;
}

long requestCupsOverWiFi(void){
  Fstr* COMMAND_URLS[1] = { F("/mepro/command_0001.json") };
  
  long WiFicups = 0;
  
  //Jos ollaan tarkistettu kupit äskettäin, ei tarkisteta vielä.
  if(CUP_CHECK_TIME && ((millis() - CUP_CHECK_TIME) < CHECK_INTERVAL)){
    return 0;
  }
  
  Serial.println("");
  Serial.println("-- Checking cups over WiFi --");
  
  // Test if module is ready
  
  while(!wifi.hardReset()) {
    #if WIFI_DEBUG
    Serial.print(F("Hard reset... "));
    Serial.println(F("no response from module."));
    #endif
  }
  #if WIFI_DEBUG
  Serial.println(F("Hard reset OK."));
  #endif
  
  while(!wifi.softReset()) {
    #if WIFI_DEBUG
    Serial.print(F("Soft reset... "));
    Serial.println(F("no response from module."));
    #endif
  }
  #if WIFI_DEBUG
  Serial.println(F("Soft reset OK."));
  #endif

  //Check firmware version only in debug mode
  #if WIFI_DEBUG
  Serial.print(F("Checking firmware version... "));
  wifi.println(F("AT+GMR"));
  if(!wifi.readLine(buffer, sizeof(buffer))) {
    Serial.print(F("Error.\n"));
  }
  else {
    Serial.println(buffer);
    wifi.find(); // Discard the 'OK' that follows
  }
  #endif
  
  Serial.print(F("Connecting to WiFi... "));
  while(!wifi.connectToAP(F(ESP_SSID), F(ESP_PASS))) {
    #if WIFI_DEBUG
    Serial.print(F("Connection timeout.\nRetry connecting to WiFi... "));
    #endif
  }
  
  // IP addr check isn't part of library yet, but
  // we can manually request and place in a string.
  #if WIFI_DEBUG
  Serial.print(F("OK.\nChecking IP addr... "));
  #endif
  wifi.println(F("AT+CIFSR"));
  if(!wifi.readLine(buffer, sizeof(buffer))) {
    #if WIFI_DEBUG
    Serial.print(F("Error.\n"));
    #endif
  }
  else {
    #if WIFI_DEBUG
    Serial.println(buffer);
    #endif
    wifi.find(); // Discard the 'OK' that follows
  }
  
  Serial.print(F("OK.\nConnecting to host... "));
  while(!wifi.connectTCP(F(HOST), PORT)) {
    #if WIFI_DEBUG
    Serial.print(F("Connection timeout.\nRetry connecting to host... "));
    #endif
  }  
  
  Serial.print(F("OK.\nRequesting page... "));
  while(!wifi.requestURL(COMMAND_URLS[0])) {
    #if WIFI_DEBUG
    Serial.print(F("Connection timeout.\nRetry requesting page... "));
    #endif
  }
  
  Serial.print("OK.\nSearching for cups... ");
  // Search for a phrase in the open stream.
  // Must be a flash-resident string (F()).
  if (wifi.find(F("AMNT:"), true)) { 
    for (int ii = 0; ii < data_length; ii++) {
      data[ii] = 0x00;
    }
    int i = 0;
    int counter = 0;
    while(Serial3.peek() != '\n') {
      while(Serial3.peek() == -1) {
        counter++;
        if(counter<0) break; //Timeout after 30k loops
      }
      counter = 0; //reset timeout
      data[i++] = Serial3.read();
      if(i == data_length) { //watch the buffer
        break;
      }
    }
    WiFicups = atoi(data);
    Serial.println(F("FOUND!"));
  }
  else { // Amount of cups not found
    Serial.println(F("not found."));
  }
  
  wifi.closeTCP();
  CUP_CHECK_TIME = millis();
  
  Serial.println("-- WiFi check ended --");
  Serial.print("Cups of coffee to make: ");
  
  return WiFicups;
}

void manualMode()
{
  Serial.println("");
  Serial.println("Manual mode - Turn the coffee maker off to return to automatic mode.");
  pressPowerButton();
  while (digitalRead(BUTTON) == HIGH) {}
  while (digitalRead(BUTTON) == LOW) {}
  pressPowerButton();
  while (digitalRead(BUTTON) == HIGH) {}
  while(Serial.available() > 0) {
    Serial.read();
  }
  Serial.print("Cups of coffee to make: ");
}

void pressPowerButton()
{
  digitalWrite(RELAY, LOW);
  delay(1000);
  digitalWrite(RELAY, HIGH);
  delay(1000);
}

void doseCoffeeGrounds(long cups)
{
  notAborted = 1;
  Serial.println("Dosing coffee grounds...");
  Serial.println("If you want to abort, press 'Y' in the keyboard or power button in the coffee machine.");
  for (cups; cups > 0; cups--) {
    motor.step(25, BACKWARD, DOUBLE);
    delay(1000);
    if (Serial.read() == 'Y' || digitalRead(BUTTON) == HIGH) {
      motor.release();
      notAborted = 0;
      pressPowerButton();
      Serial.println("Aborted");
      while (digitalRead(BUTTON) == HIGH) {}
      break;
    }
  }
  motor.release();
  if (notAborted) {
    Serial.println("Coffee grounds dosed");
  }
  while(Serial.available() > 0) {
    Serial.read();
  }
}

void fillTank(long cups)
{
  if (cups == 1) {
    time = 23000;
  }
  if (cups == 2) {
    time = 45000;
  }
  if (cups == 3) {
    time = 67000;
  }
  if (cups == 4) {
    time = 86000;
  }
  if (cups == 5) {
    time = 103000;
  }
  if (cups == 6) {
    time = 121000;
  }
  if (cups == 7) {
    time = 138000;
  }
  if (cups == 8) {
    time = 155000;
  }
  if (cups == 9) {
    time = 175000;
  }
  if (cups == 10) {
    time = 195000;
  }
  notAborted = 1;
  Serial.println("Filling tank...");
  Serial.println("If you want to abort, press 'Y' in the keyboard or power button in the coffee machine.");
  digitalWrite(PUMP, LOW);
  for (int i = 0; i < time; i++) {
    delay(1);
    if (Serial.read() == 'Y' || digitalRead(BUTTON) == HIGH) {
      digitalWrite(PUMP, HIGH);
      notAborted = 0;
      pressPowerButton();
      Serial.println("Aborted");
      while (digitalRead(BUTTON) == HIGH) {}
      break;
    }
  }
  digitalWrite(PUMP, HIGH);
  if (notAborted) {
    Serial.println("Tank filled");
  }
  while(Serial.available() > 0) {
    Serial.read();
  }
}

void waitThatCoffeeIsReady()
{
  time = 600000;
  notAborted = 1;
  Serial.println("Waiting that coffee is ready...");
  Serial.println("If you want to abort, press 'Y' in the keyboard or power button in the coffee machine.");
  for (int i = 0; i < time; i++) {
    delay(1);
    if (Serial.read() == 'Y' || digitalRead(BUTTON) == HIGH) {
      notAborted = 0;
      pressPowerButton();
      Serial.println("Aborted");
      while (digitalRead(BUTTON) == HIGH) {}
      break;
    }
  }
  if (notAborted) {
    Serial.println("Coffee is ready!");
  }
  while(Serial.available() > 0) {
    Serial.read();
  }
}

void waitForAutoOff()
{
  time = 1800000;
  notAborted = 1;
  Serial.println("Waiting for Auto Off...");
  Serial.println("If you want to abort, press 'Y' in the keyboard or power button in the coffee machine.");
  for (int i = 0; i < time; i++) {
    delay(1);
    if (Serial.read() == 'Y' || digitalRead(BUTTON) == HIGH) {
      notAborted = 0;
      pressPowerButton();
      Serial.println("Aborted");
      while (digitalRead(BUTTON) == HIGH) {}
      break;
    }
  }
  if (notAborted) {
    Serial.println("Coffee machine is turned off automatically");
  }
  while(Serial.available() > 0) {
    Serial.read();
  }
}
