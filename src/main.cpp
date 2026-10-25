#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>

#define LED_RED 15
#define LED_GREEN 12
#define LED_BLUE 13

#define USE_SERIAL Serial

ESP8266WiFiMulti WiFiMulti;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

enum State
{
  SETUP_WIFI,
  RUNNING
};

State cur_state = SETUP_WIFI;

unsigned long last_time_setup = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    USE_SERIAL.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    webSocket.sendTXT(num, "Connected");
  }
  break;
  case WStype_TEXT:
    USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

    if (payload[0] == '#')
    {
      uint32_t rgb = (uint32_t)strtol((const char *)&payload[1], NULL, 16);

      analogWrite(LED_RED, ((rgb >> 16) & 0xFF));
      analogWrite(LED_GREEN, ((rgb >> 8) & 0xFF));
      analogWrite(LED_BLUE, ((rgb >> 0) & 0xFF));
    }
    break;
  }
}

void setup()
{
  USE_SERIAL.begin(115200);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  digitalWrite(LED_RED, 1);
  digitalWrite(LED_GREEN, 1);
  digitalWrite(LED_BLUE, 1);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("", "");

  // Запуск WebSocket-сервера
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if (MDNS.begin("esp8266"))
  {
    USE_SERIAL.println("MDNS responder started");
  }

  server.on("/", []()
            { server.send(200, "text/html", "<html><head><script>var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);connection.onopen = function () {  connection.send('Connect ' + new Date()); }; connection.onerror = function (error) {    console.log('WebSocket Error ', error);};connection.onmessage = function (e) {  console.log('Server: ', e.data);};function sendRGB() {  var r = parseInt(document.getElementById('r').value).toString(16);  var g = parseInt(document.getElementById('g').value).toString(16);  var b = parseInt(document.getElementById('b').value).toString(16);  if(r.length < 2) { r = '0' + r; }   if(g.length < 2) { g = '0' + g; }   if(b.length < 2) { b = '0' + b; }   var rgb = '#'+r+g+b;    console.log('RGB: ' + rgb); connection.send(rgb); }</script></head><body>LED Control:<br/><br/>R: <input id=\"r\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" oninput=\"sendRGB();\" /><br/>G: <input id=\"g\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" oninput=\"sendRGB();\" /><br/>B: <input id=\"b\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" oninput=\"sendRGB();\" /><br/></body></html>"); });

  server.begin();

  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);

  digitalWrite(LED_RED, 0);
  digitalWrite(LED_GREEN, 0);
  digitalWrite(LED_BLUE, 0);

  USE_SERIAL.println("Connecting to WiFi...");
}

unsigned long last_10sec = 0;
unsigned int counter = 0;

void loop()
{
  unsigned long t = millis();

  switch (cur_state)
  {
  case SETUP_WIFI:
    // Не блокуємо виконання! Перевіряємо статус у кожному проході loop()
    if (WiFiMulti.run() == WL_CONNECTED)
    {
      USE_SERIAL.println("\nWiFi connected successfully!");
      USE_SERIAL.print("IP address: ");
      USE_SERIAL.println(WiFi.localIP());
      cur_state = RUNNING;
    }
    else
    {
      // Виводимо крапку кожні 500 мс без зависання
      if (t - last_time_setup >= 500)
      {
        last_time_setup = t;
        USE_SERIAL.print(".");
      }
    }
    break;

  case RUNNING:
    webSocket.loop();
    server.handleClient();

    if ((t - last_10sec) > 10 * 1000)
    {
      counter++;
      bool ping = (counter % 2);
      int i = webSocket.connectedClients(ping);
      USE_SERIAL.printf("%d Connected websocket clients ping: %d\n", i, ping);
      last_10sec = millis();
    }
    break;

  default:
    break;
  }
}