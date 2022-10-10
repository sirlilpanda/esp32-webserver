// Load Wi-Fi library
#include <WiFi.h>
// stdlibs
#include <stdint.h>
#include <stdbool.h>


// TaskHandler to allow time checking on the
// second core
TaskHandle_t Task1;

// hardware time 
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
// time counters
volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

// name of network
const char* ssid = "ESP_32_accessPoint";

// web server with on port 80 (http)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

#define WAITING 2

// struct for the timing pin
typedef struct {
  const uint8_t pin_number;
  uint8_t pin_state;
  uint64_t end_time; 
}Timed_Pin_t;

//size of the amount of timed pins
const size_t pin_amount = 2;

// timed pins array
Timed_Pin_t pins[pin_amount] = {
  (Timed_Pin_t) {.pin_number = 26, .pin_state = 1, .end_time = 0},
  (Timed_Pin_t) {.pin_number = 27, .pin_state = 1, .end_time = 0}
};

// checks all the timed pin to see if any must be changed
void checktimes(uint32_t currtime){
  for(size_t i = 0; i < pin_amount; i++){
    if (pins[i].pin_state == WAITING){
      Serial.println("========");
      Serial.println(pins[i].pin_state);
      Serial.println(pins[i].pin_number);
      Serial.println(pins[i].end_time);
      Serial.println(currtime);
      Serial.println("========");
      if (pins[i].end_time <= currtime){
        pins[i].end_time = 0;
        digitalWrite(pins[i].pin_number, HIGH);
        pins[i].pin_state = 1;
      }
    }
  }
};

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// user:pass - "dXNlcjpwYXNz"
// used for password log in
const char* base64Encoding = "dXNlcjpwYXNz";  

// used to keep time
void ARDUINO_ISR_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

// sends the html to the client
void send_button_page(WiFiClient client){
  // html header
  client.println("<!DOCTYPE html><html lang=\"en\">");
  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<link rel=\"icon\" href=\"data:,\">");
  // CSS to style the on/off buttons 
  // Feel free to change the background-color and font-size attributes to fit your preferences
  
  // style sheet
  client.println("<style>");

  client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
  client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; border-radius: 9999px;");
  client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
  client.println(".button2 {background-color: #555555;}");
  client.println("</style></head>");
  // \style sheet
  
  // Web Page Heading
  client.println("<body><h1>USB Power controller hub</h1>");
  client.println("<body><h1>USB states:</h1>");

  //adds the buttons to the web page  
  send_button_states(client, pins[0].pin_state, pins[0].pin_number);
  send_button_states(client, pins[1].pin_state, pins[1].pin_number);

  // closes of the final tags
  client.println("</body></html>");

  // ends response with a new line
  client.println();
}
//creates the html and css for the current button states
void send_button_states(WiFiClient client, uint8_t pinState, uint8_t pinNumber){

  if (pinState == 1) {
    client.println("<p>USB "+String(pinNumber-25)+" State on </p>");
    client.println("<p><a href=\"/"+String(pinNumber)+"/on\"><button class=\"button\">ON</button></a></p>");
    client.println("<p><a href=\"/"+String(pinNumber)+"/hour_on\"><button class=\"button\">ON for hour</button></a></p>");
  } else {
    client.println("<p>USB "+String(pinNumber-25)+" State off </p>");
    client.println("<p><a href=\"/"+String(pinNumber)+"/off\"><button class=\"button button2\">OFF</button></a></p>");
  }
}

// checks the html header for what to do
void check_header(){
  // turns the GPIOs on and off
  if (header.indexOf("GET /26/on") >= 0) {

    pins[0].pin_state = 0;
    Serial.println("GPIO 26 on");
    digitalWrite(pins[0].pin_number, LOW);
  
  } else if (header.indexOf("GET /26/off") >= 0) {
  
    pins[0].pin_state = 1;
    Serial.println("GPIO 26 off");
    digitalWrite(pins[0].pin_number, HIGH);
  
  } else if (header.indexOf("GET /27/on") >= 0) {
 
    pins[1].pin_state = 0;
    Serial.println("GPIO 27 on");
    digitalWrite(pins[1].pin_number, LOW);
 
  } else if (header.indexOf("GET /27/off") >= 0) {
 
    pins[1].pin_state = 1;
    Serial.println("GPIO 27 off");
    digitalWrite(pins[1].pin_number, HIGH);
 
  } else if (header.indexOf("GET /26/hour_on") >= 0){
  
    Serial.println("GPIO 26 on for an hour");
    digitalWrite(pins[0].pin_number, LOW);
    pins[0].pin_state = WAITING;
    pins[0].end_time = 10000+lastIsrAt;
  
  } else if (header.indexOf("GET /27/hour_on") >= 0){
      
    Serial.println("GPIO 27 on for an hour");
    digitalWrite(pins[1].pin_number, LOW);
    pins[1].pin_state = WAITING;
    pins[1].end_time = 10000+lastIsrAt;
  
  }
}

// updates the current timer and checks to see if pins need to be turned off
void update_and_check_timers(void* _){
  // ==== timer shit ======
  Serial.println("started task");
  Serial.print("checking.");
  for(;;){
    // If Timer has fired
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
      // Read the interrupt count and time
      portENTER_CRITICAL(&timerMux);
      portEXIT_CRITICAL(&timerMux);    
      checktimes(lastIsrAt);

    }
    delay(10); // stops the code from running too fast
  }
  // ==== timer shit ======
}

// setup
void setup() {
  Serial.begin(115200);

  //============ pin set up ==============
  pinMode(pins[0].pin_number, OUTPUT);
  pinMode(pins[1].pin_number, OUTPUT);
  digitalWrite(pins[0].pin_number, HIGH);
  digitalWrite(pins[1].pin_number, HIGH);
  //============/ pin set up ==============


  //============ wifi set up ==============
  WiFi.softAP(ssid);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  //============/ wifi set up ==============
  
  //============ timer set up ==============
  timerSemaphore = xSemaphoreCreateBinary();
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);
  //starts update_and_check_timers on core 0 as loop is on core 1
  xTaskCreatePinnedToCore(
                    update_and_check_timers,  /* Task function. */
                    "update_and_check_timers",/* name of task. */
                    10000,                    /* Stack size of task */
                    NULL,                     /* parameter of the task */
                    1,                        /* priority of the task */
                    &Task1,                   /* Task handle to keep track of created task */
                    0                         /* pin task to core 0 */    
  );          
  //delay to make sure task has created and started         
  delay(500);

  //============/ timer set up ==============

}


void loop(){
  // ==== web server code ======
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // check base64 encode for authentication
            // Finding the right credentials
            if (header.indexOf(base64Encoding)>=0)
            {
            
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              check_header();
              send_button_page(client);
              break;
            }
            else{
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Secure\"");
              client.println("Content-Type: text/html");
              client.println();
              client.println("<html>Authentication failed</html>");
            }
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear header
    header = "";

    // Close connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  // ==== web server code ======
}