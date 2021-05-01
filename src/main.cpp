
//==============================================================
//  Original example by Rui Santos
//  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-websocket-server-arduino/
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  Changed and extended to handle multiple gpio pins
//  on either the ESP8266 or ESP32 nodeMCUs - by gsb.
//

#include <Arduino.h>
#include <credentials.h>
#ifdef ARDUINO_ARCH_ESP32
	#include <WiFi.h>
	#include <AsyncTCP.h>
#else
	#include <ESP8266WiFi.h>
	#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <vector>

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Set up project data structure for gpios in use.
class gpioObj;  // simple gpio object class
#define gpio_t gpioObj*  // shorthand for pointer to an instantiated gpio object.
std::vector<gpio_t> gpio_objs;  // List of all gpios defined in the project.
class gpioObj {  // Simple gpio object definition class.
  public:
    // Data values storage associated with a gpio object.
    String  name;
    uint8_t pin;
    uint8_t state;
    // Constructor. Value initializations and gpio object pointer storage in list.
    gpioObj(String _name, uint8_t _pin, uint8_t _state=2) {
      name = _name;
      pin = _pin;
      state = _state<2?_state:digitalRead(_pin); // If 0 or 1, read local state.
      gpio_objs.push_back(this); // Add reference to 'this' an instantiated gpio object, to 'gpios' list.
      pinMode(_pin, OUTPUT);
      digitalWrite(_pin,state);
    };
    ~gpioObj(); // not meant to be de-constructed
};

// An HTML encoded gpio-switch template for inclusion into the web-page.
PROGMEM const char HTML_CARD[] = "<div id=\"%s\" class=\"%s\" onclick=\"onToggle(this)\"><h2>Toggle Switch for %s</h2></div>";

// The switches web-page template saved here in PROGMEM.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>WebSocket Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html{font-family:Arial,Helvetica,sans-serif;text-align:center}
    body{margin:0}
    header{overflow: hidden;background-color:#143642}
    h1{font-size:1.8rem;color:white}
    content{padding:30px;max-width:600px;margin:0 auto;padding:10px 0}
    .ON,.OFF{background-color:#F0F8FF;box-shadow:2px 2px 12px 1px rgba(140,140,140,.5);margin:20px 0;border:1px solid #6495ED;border-radius:12px;cursor:pointer}
    .ON h2{color:rgba(0,128,0,.7)}
    .OFF h2{color:rgba(255,0,0,.7)}
    .ON:hover h2{color:green}
    .OFF:hover h2{color:red}
    h2{font-size:1.5rem;font-weight:bold;color:#143642;line-height:1.8}
  </style>
  <script>
    var websocket,gateway=`ws://${window.location.hostname}/ws`;
    function onOpen(event){console.log('Connection opened');}
    function onClose(event){console.log('Connection closed');setTimeout(initWebSocket,2000);}
    function onToggle(el){console.log(">>"+el.id+"/toggle");websocket.send(el.id+"/toggle");}
    function onMessage(event){
      console.log("<< "+event.data);
      var pos=event.data.indexOf("/");
      var gpio=event.data.substring(0,pos);
      var state=event.data.substring(pos+1);
      document.getElementById(gpio).setAttribute("class",(state=="1"?"ON":"OFF"));
    }
    function initWebSocket(){
      websocket=new WebSocket(gateway);
      websocket.onopen=onOpen;
      websocket.onclose=onClose;
      websocket.onmessage=onMessage;
    }
  </script>
</head>
<body onLoad="initWebSocket();">
  <header><h1>WebSocket Server</h1></header>
  <content>%insert%</content>
</body>
</html>
)rawliteral";

// Send formatted gpio state data to the web-page as: gpio_name/state (0,1)
void send_gpio_state(gpio_t gpio) {
  String msg = gpio->name + F("/") + (String)gpio->state; // Format the message.
  Serial.printf(">> %s\n", msg.c_str()); // Echo outbound message sent to the web-page.
  ws.textAll(String(msg)); // Send the formatted message data to the web-page via the WebSocket.
}

// Get and process data from the web-page.
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // set terminator character
    Serial.printf("<< %s\n", data); // Echo input message from the web-page.

    // Parse the input message into its parts and then process.
    String msg = (char*)data; // Original message. Next, find the slash.
    int pos = msg.indexOf('/'); // Expected format is gpio_name/value (0,1)
    String name = msg.substring(0, pos); // grab the name, and the
    String cmd = msg.substring(pos+1);   // rab the command (example - toggle)

    // Find the named gpio object, if defined above and saved in the 'gpios' vector.
    for (size_t ndx=0; ndx < gpio_objs.size(); ndx++) {
      if (name.equals(gpio_objs[ndx]->name)) { // OK, found.
        // Command case check should there be more than just 'toggle'.
        if (cmd.equals("toggle")) {
          gpio_t gpio_obj = gpio_objs[ndx]; // gpio_obj shorthand for gpio_objs[ndx].
          digitalWrite(gpio_obj->pin, !(digitalRead(gpio_obj->pin))); // Toggle the pin.
          // The pin, LED_BUILTIN, has inverted states. HIGH-OFF & LOW-ON, so invert the state value now, as needed.
          gpio_obj->state = gpio_obj->pin!=LED_BUILTIN?digitalRead(gpio_obj->pin):!(digitalRead(gpio_obj->pin));
          send_gpio_state(gpio_obj); // Finally, update the web-page with new setting.
          return;
        }
      }
    }
    // If not processed above, send an error message to the Serial monitor.
    Serial.printf("Error: unknown message received: %s\n", msg.c_str());
  }
}

void onEvent(AsyncWebSocket *server,
      AsyncWebSocketClient *client,
      AwsEventType type,
      void *arg, uint8_t *data, size_t len)
{
  switch (type) {
    case WS_EVT_CONNECT:
      ws.cleanupClients(); // Deletes oldest client if too many;
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      ws.cleanupClients(); // Deletes oldest client if too many;
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
      Serial.println("ponged");
      break;
    case WS_EVT_ERROR:
      Serial.println("errored");
      break;
  }
}

// Because only the ESP has the gpio objects defined within, we build
// an html insert for the web-page and put it into the template
// as web-page is served up to the browser,using the gpio data.
// Each line is built as an html formatted "div tag" like:
// <div id="gpio2" class="OFF" onclick="toggle(this)"><h2>Toggle Switch for gpio2</h2></div>
// where:  'gpio2' is the switch name and 'OFF' is the current switch state.
// "toggle(this)" is the element's click-handler method defined in the
// '<script>' section of the web-page content above.
String build_html_insert() {
  String insert = ""; // Initialize the insert as a blank string.
  char buffer[strlen_P(HTML_CARD) + 64]; //...tempporary work-space to build a 'switch' line.
  for (size_t ndx=0; ndx < gpio_objs.size(); ndx++) { //...for each gpio switch defined
    // build the appropriate html line in the tempporary buffer.
    snprintf_P(buffer, sizeof(buffer), HTML_CARD,
      gpio_objs[ndx]->name.c_str(),
      (gpio_objs[ndx]->pin==LED_BUILTIN?  // LED_BUILTIN has inverted states ...
        gpio_objs[ndx]->state?"OFF":"ON": // so, reverse states as appropriate.
          gpio_objs[ndx]->state?"ON":"OFF"),
      gpio_objs[ndx]->name.c_str());
    insert += String(buffer); // And finally, add each line of html switch code to the final "insert" string.
  }
  return insert; // Return the final insert string.
}

// Build the template insert return it the server request.
String process_web_page_template(const String& var){return build_html_insert();}

//...well, "start your engines" or so they say at auto race tracks.
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // Server launches the web-page upon an empty browser HTTP_GET request to the IP.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, process_web_page_template);
  });
  server.begin();
}

// Standard Arduino project Setup and Loop methods follow.
void setup(){
  Serial.begin(115200);
  // Replace with your network credentials. I include credentials.h for mine.
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("\n\nConnecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); //...pause and then try again.
    Serial.print(".");
  }
  Serial.printf("..connectd to %s!\n",WIFI_SSID);

  initWebSocket(); //...well, start your engines, maybe.

  // Definitions of all gpio switches used in this project.
  //   String Name, int Pin, int initial State
  new gpioObj("led_builtin", 2); // LED_BUILTIN has inverted states.
  new gpioObj("gpio4", 4, 1); // Switch is initialized to ON.
  new gpioObj("kitchen", 12); // No initial state is provided, so read the pin.

  Serial.println(WiFi.localIP()); //...who am I! Access IP address.
  Serial.println("Setup complete.\nEnter the IP address in the browser to start the web-page.");
}

void loop() { }