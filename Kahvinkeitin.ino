#include <AFMotor.h>
#include <Adafruit_ESP8266.h>
#include <SoftwareSerial.h>

AF_Stepper motor(200, 1); //Init stepper motor

//Coffee maker pin definitions
#define BUTTON  31
#define RELAY  33
#define PUMP  35

//ESP Pin definitions
#define ESP_RX   7
#define ESP_TX   6
#define ESP_RST  8

SoftwareSerial softser(ESP_RX, ESP_TX); // INIT ESP
Adafruit_ESP8266 wifi(&softser, &Serial, ESP_RST); //&Serial refers to hardware serial

#define ESP_SSID "aalto open" // Your network name here
#define ESP_PASS "" // Your network password here

#define HOST     "ojousima.net"     // Host to contact
#define PORT     80                 // 80 = HTTP default port

//ESP Constant definitions
#define CHECK_INTERVAL 60000L
#define CHANGE_RATE 1000L

long cups, input, time;
int notAborted;

void setup()
{
  Serial.begin(57600); while(!Serial); // UART serial debug

  wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready")); //WiFi firmware version
  softser.begin(9600); // Soft serial connection to ESP8266

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
  resetESP(); //It's probably crashed anyway...
  wifi.connectToAP(F(ESP_SSID), F(ESP_PASS)); // Todo: fail gracefully / retry
  wifi.connectTCP(F(HOST), PORT));// Todo: fail gracefully / retry
  wifi.requestURL(page);// Todo: fail gracefully / retry
  wifi.readNumber("AMNT", "\n");//return this

  input = 0;
  while (Serial.available() == 0) { //Switch to interrupt-based mode switching
    if (digitalRead(BUTTON) == HIGH) {
      manualMode();
    }
  }
  while (Serial.available() > 0) {
    input *= 10;
    input += Serial.read() - '0';
    delay(10);
  }
  return input;
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
