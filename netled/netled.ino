/*------------------------------------------------------------------------
  Simple ESP8266 test.  Requires SoftwareSerial and an ESP8266 that's been
  flashed with recent 'AT' firmware operating at 9600 baud.  Only tested
  w/Adafruit-programmed modules: https://www.adafruit.com/product/2282

  The ESP8266 is a 3.3V device.  Safe operation with 5V devices (most
  Arduino boards) requires a logic-level shifter for TX and RX signals.
  ------------------------------------------------------------------------*/

#include <Adafruit_ESP8266.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>

//ESP pin definitions
#define ESP_RX   7
#define ESP_TX   6
#define ESP_RST  5

//Task interval time constants, in millis()
#define CHECK_INTERVAL 60000L


//PowerLED pin definitions
#define ledPin0  9
#define ledPin1  11
#define ledPin2  10
#define NUM_LEDS 3

//Indicator led definitions
#define Indicator_WiFi  8   
#define Indicator_Power 12

//Push button pin definitions
#define plusPin  4
#define minusPin 3
#define modePin  2

//LED values
uint8_t LED_PINS[NUM_LEDS] = {ledPin0, ledPin1, ledPin2};
unsigned long LED_CHECK_TIMES   [NUM_LEDS + 1] = { 0 };//Hax to get RTC using same led function.
unsigned long LED_CONTROL_TIMES [NUM_LEDS + 1] = { 0 };
uint8_t LED_CONTROL_VALUES [NUM_LEDS] = { 0 };
uint8_t LED_WIFI_VALUES [NUM_LEDS] = { 0 };
SoftwareSerial softser(ESP_RX, ESP_TX);

// Must declare output stream before Adafruit_ESP8266 constructor; can be
// a SoftwareSerial stream, or Serial/Serial1/etc. for UART.
#define WIFI_DEBUG 0
Adafruit_ESP8266 wifi(&softser, &Serial, ESP_RST);
// Must call begin() on the stream(s) before using Adafruit_ESP8266 object.

#define ESP_SSID "aalto open" // Your network name here
#define ESP_PASS "" // Your network password here

#define HOST     "ojousima.net"     // Host to contact
#define PORT     80                     // 80 = HTTP default port

#define LED_PIN  13

char buffer[50];
char data[10];
int16_t RTC = 0000;

volatile byte mode = 0; //0 - WIFI 1 - MANUAL 2 - OFF
int16_t manual_setting = 0;
uint8_t manual_increment = 10;

void setup() {

  // Flash LED on power-up
  pinMode(LED_PIN, OUTPUT);
  pinMode(ledPin0, OUTPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  
  //Button pins
  pinMode (plusPin, INPUT_PULLUP);
  pinMode (minusPin, INPUT_PULLUP);
  pinMode (modePin, INPUT_PULLUP);
  
  pinMode (Indicator_WiFi, OUTPUT);
  pinMode (Indicator_Power, OUTPUT);
  
  // This might work with other firmware versions (no guarantees)
  // by providing a string to ID the tail end of the boot message:
  
  // comment/replace this if you are using something other than v 0.9.2.4!
  wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready"));
  
  //Set timeouts < 8000 because WDT triggers once every 8000 ms.
  wifi.setTimeouts(0,0,5000,5000);

  softser.begin(9600); // Soft serial connection to ESP8266
  Serial.begin(57600); while(!Serial); // UART serial debug

  Serial.println(F("NetLed project based on Adafruit ESP8266 Demo"));

  delay(200);
  watchdogSetup();
  digitalWrite(Indicator_Power, HIGH);
  
  
}

void loop() {
  
  //buttontest
   //digitalWrite(13, !digitalRead(plusPin)||!digitalRead(minusPin)||!digitalRead(modePin));   // turn the LED on (HIGH is the voltage level)
  /*
  WiFi mode
   */
 if(mode == 0){
   digitalWrite(Indicator_WiFi, HIGH);
   EIFR = 0x01; //clear interrupt flag
   attachInterrupt(0, toManual, FALLING);
   wdt_reset();
   
   //And this kids, is how horrible spaghetti monster is born.
   //Abuses request LEDS function to retrieve real time.
   int timeholder = requestLed(NUM_LEDS); 
   
   if(timeholder >= 0)
     RTC = timeholder;
   
  for(int index = 0; index < NUM_LEDS && (mode == 0); index++){
    int ledValue = requestLed(index);
    wdt_reset();
    if(ledValue >= 0){
      LED_WIFI_VALUES[index] = ledValue;
    }
    LED_CONTROL_VALUES[index] = LED_WIFI_VALUES[index];
  }
 }
  
  /*
    Manual mode
  */
  if(mode == 1){
    digitalWrite(Indicator_WiFi, LOW);
    digitalWrite(Indicator_Power, HIGH);
    
    if(debounce(!digitalRead(plusPin))){
      manual_setting += manual_increment;
      if(manual_setting > 255)
        manual_setting = 255; 
    }
   
    if(debounce(!digitalRead(minusPin))){
      manual_setting -= manual_increment;
      if(manual_setting < 0)
        manual_setting = 0; 
    }
    
    for(int index = 0; index < NUM_LEDS; index++){
         LED_CONTROL_VALUES[index] = manual_setting;
    }    
    
    if(debounce(!digitalRead(modePin))){
      mode = 2;
    }
  }
  /*
   * OFF mode
   */
  if(mode == 2) {
    digitalWrite(Indicator_WiFi, LOW);
    digitalWrite(Indicator_Power, LOW);
    
    for(int index = 0; index < NUM_LEDS; index++){
         LED_CONTROL_VALUES[index] = 0;
    } 
    
    if(debounce(!digitalRead(modePin))){
      mode = 0;
      digitalWrite(Indicator_WiFi, HIGH);
      digitalWrite(Indicator_Power, HIGH);
      for(int index = 0; index < NUM_LEDS; index++){
        LED_CHECK_TIMES   [index] =  0 ;
      }
    
    //Restore wifi values with state transition.  
    for(int index = 0; index < NUM_LEDS; index++){
         LED_CONTROL_VALUES[index] = LED_WIFI_VALUES[index];
    } 
 
      //Ghetto-debounce
      while(!digitalRead(modePin)){}
      delay(250);
    }
  } 

  /*
   Common to all modes
  */
  for(int index = 0; index < NUM_LEDS; index++){
    setLed(index, LED_CONTROL_VALUES[index]);
    Serial.print("Setting Led ");
    Serial.print(index+1);
    Serial.print(" to ");
    Serial.println(LED_CONTROL_VALUES[index]);
    wdt_reset();
  }
  
  Serial.print("Mode: ");
  Serial.print(mode);
  Serial.print(" Time: ");
  Serial.println(RTC); 
  wdt_reset();

}

//TODO: smoothing
int setLed(uint8_t led, uint8_t newValue){
  analogWrite(LED_PINS[led], LED_CONTROL_VALUES[led]);
  return LED_CONTROL_VALUES[led];    
}

int requestLed(uint8_t LED){
  Fstr* COMMAND_URLS[NUM_LEDS + 1] = { F("/mepro/command_1000.json"),
                                 F("/mepro/command_1001.json"), 
				 F("/mepro/command_1002.json"),
                                 F("/mepro/now.php")}; //HAX! :p
  
  int ledValue = 0;
  int ledTime = 0;
  
  if(LED_CHECK_TIMES[LED] && ((millis() - LED_CHECK_TIMES[LED]) < CHECK_INTERVAL)){
    return -1;
  }
  
  Serial.print("Checking led: ");
  Serial.println(LED);

  
  if(mode)
    return -1;
    
  // Test if module is ready
  Serial.print(F("Hard reset..."));
  if(!wifi.hardReset()) {
    Serial.println(F("no response from modulev after hard reset."));
    while(!wifi.hardReset() && !mode){ // try to recover
      digitalWrite(Indicator_WiFi, !digitalRead(Indicator_WiFi));
      delay(100);
    }
    if(mode)
      return -1;
  }
  Serial.println(F("OK."));

  Serial.print(F("Soft reset..."));
  if(!wifi.softReset()) {
    Serial.println(F("no response from module after soft reset."));
    while(!wifi.softReset() && !mode){// try to recover
      digitalWrite(Indicator_WiFi, !digitalRead(Indicator_WiFi));
      delay(100);
    }
    if(mode)
      return -1;
  }
  
#if WIFI_DEBUG
  Serial.println(F("OK."));

  Serial.print(F("Checking firmware version..."));
#endif



  wifi.println(F("AT+GMR"));
  if(wifi.readLine(buffer, sizeof(buffer))) {
    Serial.println(buffer);
    wifi.find(); // Discard the 'OK' that follows
  } else {
    Serial.println(F("error"));
  }

  Serial.print(F("Connecting to WiFi..."));
  
  wdt_reset();  
  
  if(wifi.connectToAP(F(ESP_SSID), F(ESP_PASS))) {
    digitalWrite(Indicator_WiFi, !digitalRead(Indicator_WiFi));
    if(mode)
      return -1;
    // IP addr check isn't part of library yet, but
    // we can manually request and place in a string.

#if WIFI_DEBUG
    Serial.print(F("OK\nChecking IP addr..."));
#endif
    wdt_reset();  
    wifi.println(F("AT+CIFSR"));
    if(wifi.readLine(buffer, sizeof(buffer))) {
      //Serial.println("I'm alive!");
      Serial.println(buffer);
      wifi.find(); // Discard the 'OK' that follows

      Serial.print(F("Connecting to host..."));


      byte counter = 0;
      byte ledState = 0;
      while(!wifi.connectTCP(F(HOST), PORT)) {
        digitalWrite(Indicator_WiFi, !digitalRead(Indicator_WiFi));
        if(mode)
          return -1;
        counter++;
        if(counter < 32){
          wdt_reset();
          Serial.println("WDT RESET");
        }
        
        digitalWrite(Indicator_WiFi, ledState);
        ledState = ~ledState;  

        delay(100);
      }

      wdt_reset();  

#if WIFI_DEBUG
      Serial.print(F("OK\nRequesting page..."));
#endif

      if(wifi.requestURL(COMMAND_URLS[LED])) {
        digitalWrite(Indicator_WiFi, !digitalRead(Indicator_WiFi));
        if(mode)
          return -1;
#if WIFI_DEBUG        
          Serial.println("OK\nSearching for string...");
#endif 
        wdt_reset();  
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
        
        ledValue=atoi(data);
          Serial.print(F("Led value: "));
          Serial.println(ledValue);
        } else {
          Serial.println(F("not found."));
          ledValue = -1;
        }
        //get time when to execute, 9999 for NOW/always.
        if(wifi.find(F("TIME:"), false)) { 
          for(int ii=0; ii<10; ii++){
            data[ii]=0x00;
          }
        int i = 0;

        while(softser.peek()!='\n'){
           data[i++] = softser.read();
        }
        ledTime=atoi(data);
          Serial.print(F("Start at: "));
          Serial.println(ledTime);
        } else {
          Serial.println(F("not found."));
          ledValue = -1;
        }
      } else { // URL request failed
        Serial.println(F("URL request failed"));
        return -1;
    }
    wifi.closeTCP();
    } else { // IP addr check failed
      Serial.println(F("IP addr check failed"));
      return -1;
    }
  wifi.closeAP();
  } else { // WiFi connection failed
    Serial.println(F("Wifi connection FAILED"));
    return -1;
  } 
  wdt_reset();  
  
  //Successful check, store time
  //TODO: Fix bug where checking time itself fails and others update against false time.
  LED_CHECK_TIMES[LED] = millis();
  //Is it the time?
  if(ledTime == 9999){ //9999 for ALWAYS
    Serial.print("LED: ");
    Serial.print(LED);
    Serial.print("Value: ");
    Serial.print(ledValue);
    Serial.println("Command is always on");
    return ledValue;
  }
  
  //  if this command has not been executed yet AND it should be done within a minute AND we're less than 30 minutes overdue
  if(ledTime != LED_CONTROL_TIMES[LED] && ((ledTime-RTC) < 1) && ((RTC-ledTime) < 30)){
    LED_CONTROL_TIMES[LED] = ledTime;
    Serial.print("LED: ");
    Serial.print(LED);
    Serial.print("Value: ");
    Serial.print(ledValue);
    Serial.println("Time is right");
    return ledValue;
  }
    
  Serial.print("LED: ");
  Serial.print(LED);
  Serial.print(" Value: ");
  Serial.print(ledValue);
  Serial.println(" Time mismatch");  
  return -1;
  
}

void watchdogSetup(void)
{
cli();
wdt_reset();
/*
WDTCSR configuration:
WDIE = 1: Interrupt Enable
WDE = 1 :Reset Enable
See table for time-out variations:
WDP3 = 0 :For 1000ms Time-out
WDP2 = 1 :For 1000ms Time-out
WDP1 = 1 :For 1000ms Time-out
WDP0 = 0 :For 1000ms Time-out
*/
// Enter Watchdog Configuration mode:
WDTCSR |= (1<<WDCE) | (1<<WDE);
// Set Watchdog settings:
WDTCSR = (1<<WDIE) | (0<<WDE) |
(1<<WDP3) | (1<<WDP2) | (1<<WDP1) |
(1<<WDP0);
sei();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
  digitalWrite(Indicator_Power, LOW);
  digitalWrite(Indicator_WiFi, LOW);
  asm volatile ("  jmp 0");  //Soft reset
}

uint8_t debounce(uint8_t signal){
 const long DEBOUNCE_PERIOD = 100; 
 static long lastPress = millis();
 static uint8_t pressed = 0;
 
 if(!signal) {
   if(millis()-lastPress > DEBOUNCE_PERIOD)
     pressed = 0;
     
   return 0; 
 }
 
 // Event detected  
 if(signal && !pressed){
   pressed = 1;
   lastPress = millis();
   return 1;
 }

 //Signal stays high
 lastPress = millis();
 return 0; 
  
}

void toManual(void)
{
  mode = 1;
  wdt_reset();
  digitalWrite(Indicator_WiFi, LOW);
  detachInterrupt(0);
}
