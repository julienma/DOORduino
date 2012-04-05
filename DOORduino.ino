/*
  
 DOORduino
 
 A web server awaiting connections via bookmarklet.
 Each URL verb will trigger an action (e.g. http://my.ip/url-verb/ ), depending on 
 which pin is plugged in the corresponding verb (relay, led, etc.)
 
 Created 19 Mar 2012
 by Julien Marchand
 
 */

#include <SPI.h>
#include <Ethernet.h>
// librairie pour stockage en mémoire flash programme
// attention : l'utilisation de F("chaine") necessite modification du fichier print.h
// voir : http://www.mon-club-elec.fr/pmwiki_reference_arduino/pmwiki.php?n=Main.LibrairieFlashProgramme
// + mandatory patch for latest arduino IDE: in Flash library, replace WProgram.h by Arduino.h
// see http://arduino.cc/forum/index.php/topic,83278.msg625186.html#msg625186
#include <Flash.h> 

/*
  
  SETUP
  This is were you should define URLs (e.g. http://my.ip/verb) and I/O pins assigned to them.
  Make sure to initialize PIN numbers and URL verbs in the same order so they can match together.
  The TIME gives the maximum time for which each PIN is HIGH (in milliseconds).
  
*/

const char* URL[] = { // Make sure that URL verbs are UNIQUE.
  "green",
  "red", 
  "door"};
  
const int PIN[] = { // Make sure that I/O pins are used ONCE.
  3, // 3 = green led
  5, // 5 = red led
  8}; // 8 = relay

const int TIME[] = { // in milliseconds.
  2000,
  10000,
  4000};
  
unsigned long lastActionMillis[] = { // keep track of verb's action durations. Always init with 0.
  0,
  0,
  0};

// Enter a MAC address and IP address for your controller below.
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x14, 0x36 };
IPAddress ip(192,168,0,200);

// variables for arrays' lenghts
int urlLength = 0;
int pinLength = 0;
int timeLength = 0;
int lastActionMillisLength = 0;

// variable for tracking current time
unsigned long currentMillis = 0;
  
// Initialize the Ethernet server and client libraries
EthernetServer server(1337); // port to use for server
EthernetClient client;

/* 
  
  SETUP
  
*/

void setup()
{
  
  // start the serial library
  Serial.begin(28800); 

  // make sure that PIN and URL arrays are correctly defined
  urlLength = sizeof(URL)/sizeof(char*);
  pinLength = sizeof(PIN)/sizeof(int);
  timeLength = sizeof(TIME)/sizeof(int);
  lastActionMillisLength = sizeof(lastActionMillis)/sizeof(unsigned long);

  if (pinLength != urlLength || pinLength != timeLength || pinLength != lastActionMillisLength) {
    Serial.println(F("WARNING: your PIN, URL, TIME and lastActionMillis arrays should have the same number of items."));
    Serial.println(F("End of program."));
    for(;;);
  }
  else
  {
    // display all the possible URL verbs and corresponding pins
    Serial.println(F(""));
    Serial.println(F("Available URL verb(s): "));
    for (int i=0; i<urlLength; i++) 
    {
      Serial.print(i), Serial.print(F(" - ")), Serial.print(URL[i]), Serial.print(F(" (Pin #")), 
        Serial.print(PIN[i]), Serial.print(F(", ")), Serial.print(TIME[i]), Serial.println(F(" ms)"));
    }
    Serial.println(F(""));
  }

  // start the Ethernet connection using DHCP:
  Serial.println(F("Connecting..."));
  if (Ethernet.begin(mac) != 0)
  {
    // print your local IP address:
    Serial.print(F("My IP Address: "));
    Serial.println(Ethernet.localIP());
    // start the server
    server.begin();
  }
  else
  {
    Serial.println(F("Failed to configure Ethernet using DHCP."));
  }

  // set PINs as OUTPUT
  for (int i=0; i<pinLength; i++) 
  {
    pinMode (PIN[i], OUTPUT);
  }

}

/* 
  
  LOOP
  
*/

void loop()
{

  // wait for an available client
  while(!server.available())
  {
    // Get current time
    currentMillis = millis();
    
    for (int i=0; i<pinLength; i++) 
    {      
      // Should we deactivate an ongoing action?
      if((digitalRead(PIN[i]) == HIGH) && ((currentMillis - lastActionMillis[i]) > TIME[i]))
      {
        Serial.print(F("Set pin LOW: "));
        Serial.print(PIN[i]);
        Serial.print(F(" (after "));
        Serial.print(currentMillis - lastActionMillis[i]);        
        Serial.println(F(" ms)"));
        // time out; set pin LOW
        digitalWrite(PIN[i],LOW);
      }
    }
  
  }
  
  // start server and listen for incoming clients
  startServer();
  
}

/* 
  
  START SERVER
  
*/

void startServer() 
{

  // wait for an available client
//  while(!server.available());

  // listen for incoming clients
  client = server.available();

  if (client) 
  {
    Serial.println(F("> connection"));

    // variables used for client's incoming requests
    String request = ""; // http request received
    int requestCharCount = 0; // request's characters count set to 0

    if (client.connected()) 
    {
      //Serial.println(F("-- Connection OK, waiting for request"));
  
      // while bytes are available for reading
      while (client.available())
      {
        // read next byte sent by client
        char c = client.read(); 
        requestCharCount=requestCharCount+1;
        
        // construct the "request" string with only N first characters
        // 100 is enough to analyze the verb, and this is lighter in memory
        if (requestCharCount <= 100) request = request + c; 
        //Serial.print(c);
      }

      // analyze request
      analyzeRequest(request);
      // and output HTML menu
      outputHTML();

    } 

    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop(); 
    //Serial.println(F("-- Connection closed")); 

  }

} 

/* 
  
  ANALYZE REQUEST
  
*/

void analyzeRequest(String request)
{
  // analyze incoming request
  //Serial.println(F("-- Analyzing request"));
  //Serial.println(F("String being analyzed: ")); 
  //Serial.println(request);
  //Serial.println(F("")); 

  // search for the 1st slash in the request, which should be followed by the URL verb
  int indexSlash = request.indexOf("/") + 1;
  String urlVerb = request.substring(indexSlash, indexSlash + 1);
 
  // URL verb is available
  if (urlVerb != " ") 
  {
    
    // search for all the possible URL verbs in the request
    for (int i=0; i<urlLength; i++) 
    {
      // the verb we're looking for has been found
      if (request.indexOf(URL[i]) != -1)
      {
        
        // check that the action is NOT already ongoing
        if (digitalRead(PIN[i]) == LOW)
        {     
          Serial.print(F("Found VERB \"")), Serial.print(URL[i]), 
          Serial.print(F("\" => PIN #")), Serial.print(PIN[i]), 
          Serial.print(F(" going HIGH for ")), Serial.print(TIME[i]), Serial.println(F(" ms."));
          
          // set pin HIGH
          digitalWrite(PIN[i],HIGH);
          // set action as ongoing
          lastActionMillis[i] = currentMillis;
          
          // execute only 1 verb
          break;
        } 
        else
        { 
          Serial.print(F("Action already ongoing: \"")), Serial.print(URL[i]), Serial.print(F("\" since ")), 
          Serial.print(currentMillis - lastActionMillis[i]), Serial.println(F(" ms.")); 
        }
        
      }
    }

  } // no URL verb
  else 
  {
    Serial.println(F("No URL verb given!"));
  }
 
}

/* 
  
  OUTPUT HTML
  
*/

void outputHTML()
{
  
  //Serial.println(F("-- Sending HTML output to client"));
  
  // standard http header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  // tell client that connection will be closed after output (remain open by default)
  client.println(F("Connection: close")); 
  // mandatory blank line after http header
  client.println(); 
  
  // html output
  client.println(F("<html>")); 
  client.println(F("<head>"));
  client.println(F("<meta content=\"text/html; charset=UTF-8\" http-equiv=\"Content-Type\">")); 
  client.println(F("<title>DOORduino</title>")); 
  client.println(F("</head>"));
  client.println(F("<body style=\"font-family: arial, verdana; font-size: 12px;\">")); 
  client.println(F("<br>"));
  client.println(F("<img src=\"http://assets.zendesk.com/system/logos/2001/2851/leadformance_logo2011-ECRAN.png\">"));
  client.println(F("<br>"));
  client.println(F("######### <br> ")); 
  client.println(F("DOORduino <br>")); 
  client.println(F("######### <br>")); 
  client.println(F("<br>"));
  
  client.println(F("<ul>"));
  
  for (int i=0; i<urlLength; i++) 
  {
    client.print(F("<li><a href=\"/")), client.print(URL[i]);
    client.print(F("/\" title=\"=> PIN #")), client.print(PIN[i]);
    client.print(F(" (")), client.print(TIME[i]), client.print(F(" ms)"));
    client.print(F("\">")), client.print(URL[i]), client.println(F("</a></li><br>"));
  } 
  
  client.println(F("</ul>"));      
  client.println(F("<br>"));
  client.println(F("</body>"));
  client.println(F("</html>"));
   
}
