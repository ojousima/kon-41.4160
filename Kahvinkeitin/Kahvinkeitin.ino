//#include <AFMotor.h>
#include <Adafruit_ESP8266.h>
#include <SoftwareSerial.h>

//AF_Stepper motor(200, 1); //Init stepper motor

//Coffee maker pin definitions
/*#define BUTTON  31
#define RELAY  33
#define PUMP  35*/

//ESP Pin definitions
#define ESP_TX   7
#define ESP_RX   6
#define ESP_RST  8


SoftwareSerial softser(ESP_RX, ESP_TX); // INIT ESP
Adafruit_ESP8266 wifi(&softser, &Serial, ESP_RST); //&Serial refers to hardware serial

#define ESP_SSID "aalto open" // Your network name here
#define ESP_PASS "" // Your network password here

#define HOST     "ojousima.net"     // Host to contact
#define PORT     80                 // 80 = HTTP default port

//ESP Constant definitions
#define CHECK_INTERVAL 1800000L      //Check every 30 minutes.
#define CHANGE_RATE 1000L

long CUP_CHECK_TIME = 0;
long cups, input, time;
int notAborted;
char buffer[50];
char data[10];

void setup()
{
  Serial.begin(57600); while(!Serial); // UART serial debug

  wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready")); //WiFi firmware version
  softser.begin(9600); // Soft serial connection to ESP8266

  //Pump, relay setup
  //motor.setSpeed(10);
  //pinMode(BUTTON, INPUT);
  //pinMode(RELAY, OUTPUT);
  //digitalWrite(RELAY, HIGH);
  //pinMode(PUMP, OUTPUT);
  //digitalWrite(PUMP, HIGH);
}

 void loop()
{
  Serial.print("Cups of coffee to make: ");
  //cups = askNumberOfCups();
  
  //Katsotaan pitäisikö keittää kahvia
  int check = requestCups();
  
  //Jos pitäisi, tallennetaan montako kuppia
  if(check > 0) {
      cups = check;
  }

  Serial.println(cups);
  if (cups >= 1 && cups <= 10) {
    //pressPowerButton();
    //doseCoffeeGrounds(cups);
    if (notAborted) {
 //     fillTank(cups);
    }
    if (notAborted) {
   //   waitThatCoffeeIsReady();
    }
    if (notAborted) {
    //  waitForAutoOff();
    }
  }
  else {
    Serial.println("Invalid number");
  }
}

long requestCups(void){
  Fstr* COMMAND_URLS[1] = { F("/mepro/command_0001.json") };
  
  long cups = 0;
  
  //Jos ollaan tarkistettu kupit äskettäin, ei tarkisteta vielä.
  if(CUP_CHECK_TIME && ((millis() - CUP_CHECK_TIME) < CHECK_INTERVAL)){
    return -1;
  }
  Serial.print("Checking cups");
  CUP_CHECK_TIME = millis();
  
   // Test if module is ready
  Serial.print(F("Hard reset..."));
  if(!wifi.hardReset()) {
    Serial.println(F("no response from module."));
    for(;;);
  }
  Serial.println(F("OK."));

  Serial.print(F("Soft reset..."));
  if(!wifi.softReset()) {
    Serial.println(F("no response from module."));
    for(;;);
  }
  Serial.println(F("OK."));

  Serial.print(F("Checking firmware version..."));
  wifi.println(F("AT+GMR"));
  if(wifi.readLine(buffer, sizeof(buffer))) {
    Serial.println(buffer);
    wifi.find(); // Discard the 'OK' that follows
  } else {
    Serial.println(F("error"));
  }

  Serial.print(F("Connecting to WiFi..."));
  if(wifi.connectToAP(F(ESP_SSID), F(ESP_PASS))) {

    // IP addr check isn't part of library yet, but
    // we can manually request and place in a string.
    Serial.print(F("OK\nChecking IP addr..."));
    wifi.println(F("AT+CIFSR"));
    if(wifi.readLine(buffer, sizeof(buffer))) {
      Serial.println(buffer);
      wifi.find(); // Discard the 'OK' that follows

      Serial.print(F("Connecting to host..."));
      while(!wifi.connectTCP(F(HOST), PORT)) {}
        Serial.print(F("OK\nRequesting page..."));
        if(wifi.requestURL(COMMAND_URLS[0])) {
          Serial.println("OK\nSearching for string...");
          // Search for a phrase in the open stream.
          // Must be a flash-resident string (F()).
          if(wifi.find(F("AMNT:"), false)) { 
            for(int ii=0; ii<10; ii++){
              data[ii]=0x00;
            }
            int i = 0;

            while(softser.peek()!='\n'){
                data[i++] = softser.read();
            }
            cups=atoi(data);
            Serial.print(F("Cups to make: "));
            Serial.println(cups);
          } else {
            Serial.println(F("not found."));
            cups = -1;
          }
        } else { // URL request failed
          Serial.println(F("URL request failed"));
        }
        wifi.closeTCP();
    } else { // IP addr check failed
      Serial.println(F("IP addr check failed"));
    }
    wifi.closeAP();
  } else { // WiFi connection failed
    Serial.println(F("Wifi connection FAILED"));
  } 
  
  return cups;
  
}
/*
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
}*/
