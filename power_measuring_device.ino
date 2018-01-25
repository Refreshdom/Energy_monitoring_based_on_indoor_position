
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//Pin define
//static const uint8_t D0   = 16;
//static const uint8_t D1   = 5;
//static const uint8_t D2   = 4;
//static const uint8_t D3   = 0;
//static const uint8_t D4   = 2;
//static const uint8_t D5   = 14;
//static const uint8_t D6   = 12;
//static const uint8_t D7   = 13;
//static const uint8_t D8   = 15;
//static const uint8_t D9   = 3;
//static const uint8_t D10  = 1;

#define touchPin 12
#define relayPin 14

const char* ssid = "ssid@unifi";
const char* password = "password0";
//const char* mqtt_server = "192.168.0.239";
const char*mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

//const unsigned long publishInterval = 60L*1000L;  //publish every 1minute
const unsigned long publishInterval = 10L * 1000L;  //publish every 10second
unsigned long lastMsg = 0;

//TouchSensor Setting
bool touchOff = true;  //always off first
bool touchFlag = LOW;  //sensor trigger
unsigned long debounce = 0;

//Switch Relay Setting
bool relayOff = true; //always off first
unsigned long modTimer = 20L * 1000L; //delay for relay to switch off (initial set 20second)
unsigned long cooldown = 0; //reset count down if user go out and back into location (no cooldown)

String users = "0"; //list of user
String foundId = "";  //returned user Id
String foundLoc = "";   //returned user location
bool userFlag = false;  //initiate cooldown when userFlag not found

//Thingspeak setting
String apikey = "FF1L7B0Z41CRJ08U";
String myRoom = "Living"; //Power measuring device current location
String msg = "";  //returned measuring device msg
String pmode = "3"; //default 3 or in the future set to one

//UART VARIABLE
//buffer
bool serialComplete = false;
String message = "";
char command = ' ';
int inchar = 0;
int endPos = 0;
//receive
String pstatus = "0"; //0: switched off, 1: switched on
String preal = "0", pvoltage = "0", pfactor = "0", pcurrent = "0";

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void serialEvent() {
  //Serial UART function
  while (Serial.available()) {
    delayMicroseconds(100);
    inchar = Serial.read();
    message += (char)inchar;
    if (inchar == '\n') {
      serialComplete = true;
    }
  }

}

void addUser(String uid) {
  //add total user found
  users += "+";
  users += uid;
}

void delUser(String uid) {
  //remove user when not found
  int a = users.indexOf(uid);
  users.remove(a - 1, uid.length() + 1);
}

void delimiter(String message) { // msg format: UID+CURLOC
  // For loop which will separate the String in parts
  // and assign them the the variables we declare
  for (int i = 0; i < message.length(); i++) {
    if (message.substring(i, i + 1) == "+") {
      foundId = message.substring(0, i);
      foundLoc = message.substring(i + 1);
      break;
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("testtestclientee")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      
      client.subscribe("digitalHome/app");
      client.subscribe("digitalHome/mod");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

void callback(char* topic, byte* payload, unsigned int length) {
  //MQTT callback function
  String payloadMsg;
  //  Serial.print("Message arrived [");
  //  Serial.print(topic);
  //  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    payloadMsg += (char)payload[i];
  }

  if ( String(topic).indexOf("app")) {
    //topic "app" indicate msg of user location
    delimiter(payloadMsg); //parse data
  }
  if (users.indexOf(foundId) >= 0) {
    if (myRoom.indexOf(foundLoc) != 0) {
      delUser(foundId);
    }
  }
  else {
    //    Serial.print("Room detected : ");
    //    Serial.println(foundLoc.indexOf(myRoom));
    if (myRoom.indexOf(foundLoc) == 0) {
      addUser(foundId);
    }
  }
  
  if ( String(topic).indexOf("mod")) {
    //topic "mod" indicate setting for automatic switch off timer
    delimiter(payloadMsg); //prase data
  }
  if ( foundLoc == "Living" ) {
    Serial.println(foundId.toInt());
    switch ( foundId.toInt() ) {
      case 0:
        modTimer = 5L * 1000L;
        pmode = "0";
        break;
      case 1:
        pmode = "1";
        modTimer = 20L * 1000L;
        break;
      case 2:
        pmode = "2";
        modTimer = 600L * 1000L;
        break;
      default:
        modTimer = 1000000L;
        pmode = "3";
        break;
    }
    Serial.println(pmode);
  }
}

void setup() {
  delay(500); //wait for hardware initialize completely
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayOff);
  pinMode(touchPin, INPUT);

  Serial.begin(38400);  //initialize serial port
  setup_wifi(); //connect to internet
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void loop() {
  //Touch Sensor 
  touchFlag = digitalRead(touchPin);  //poll trigger

  if ( touchFlag == HIGH ) {   //if high check debounce

    if ( (millis() - debounce) > 230 ) { //200ms
      debounce = millis();
      switch ( relayOff ) {
        case true:
          relayOff = false;
          pstatus = "1";
          cooldown = millis();
          userFlag = true;
          break;
        case false:
          relayOff = true;
          cooldown = 0;
          userFlag = false;
          break;
        default:
          break;
      }

    }
    bool norep = digitalRead(touchPin);

    if ( norep == HIGH ) debounce = millis(); //no repeat trigger
  }
  digitalWrite(relayPin, relayOff);
  
  //UART read
  serialEvent();
  if (serialComplete) {
    //set buffer size
    command = message.charAt(0);  //head
    endPos = message.indexOf("\n"); //tail
    Serial.println(message);
    
    //parse power data from message
    preal = message.substring(message.indexOf("A") + 1, message.indexOf("B"));
    pvoltage = message.substring(message.indexOf("B") + 1, message.indexOf("C"));
    pcurrent = message.substring(message.indexOf("C") + 1, message.indexOf("D"));
    pfactor = message.substring(message.indexOf("D") + 1, endPos);
    
    message = "";
    serialComplete = false;
  }
  //Serial read complete
  if (!client.connected()) {
    reconnect();
  }

  //Check for relay status
  if ( relayOff ) {
    //if off-> clear preal and pstatus
    preal = "0";
    pstatus = "0";
  }
  else {
    if (users.indexOf("+") < 0 ) {  //when no ppl
      if (userFlag) {               //switch off the power
        userFlag = false;           //
      }
      else {
        //start timer to count down switch off
        if (millis() - cooldown > modTimer) {
          //Serial.println("off");
          relayOff = true;
          preal = "0";
          pstatus = "0";
        }
      }
    }
    else {
      cooldown = millis();
      userFlag = true;
    }
  }

  long now = millis();//track current timer
  if (now - lastMsg > publishInterval) {  
    //if the timer exceed publishInterval
    //publish the data                    
    lastMsg = now;  //reset timer
    //concatenate msg
    msg += pstatus;  //power status
    msg += "_";
    msg += preal;
    msg += "_";
    msg += pmode;
    msg += "_";
    msg += users;

    String topic = "digitalHome/devices/";
    topic += apikey;

    char pubMsg[msg.length()];
    msg.toCharArray(pubMsg, msg.length() + 1);

    //Create char[] to store topic
    char pubTopic[topic.length()];
    topic.toCharArray(pubTopic, topic.length() + 1);

    client.publish(pubTopic, pubMsg);
    msg = ""; //clear buffer
  }

  client.loop();  //MQTT check for payload
}
