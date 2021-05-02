# ESP_WebSocket_Server_Example
Examples an HTML web-page for ESP GPIO binary-switch controls via an Async WebSockets Server.

## What is it?
This is a personal project to help another change and extend the "Random Nerd Tutorials - [ESP8266 NodeMCU WebSocket Server: Control Outputs (Arduino IDE)](https://randomnerdtutorials.com/esp8266-nodemcu-websocket-server-arduino/)" to better meet the needs of their project.

It implements extensive changes to the original code keeping its underlying concepts, approach and code structure. It does not advance the original intent beyond extending the one gpio switch example to multiple gpio switches. Primarily, the ESP code now contains list-management for one-or-more controllable gpios, builds the web-page upon it's initialization and caries out a simple gpio state toggle upon user request via the web-page. It also recognizes and compensates for the LED_BUILTIN (GPIO02) which has state-inversion, HIGH as OFF and LOW as ON. And of course, the web_page has changed considerably to accomodate multipe switches instead of the original single one.

It is presented here as a platformio project. To convert it to an Arduino IDE example, simply change main.cpp to main.ino. Be sure to get the Arduino settings correct for the ESP build and insure to add the necessary libraries, ESPAsyncTCP-esphome and ESPAsyncWebServer-esphome, via the Adruino Library Manager.

