/*

Created by laarky (rhz17) (Victor Machado)

*/

#include <Arduino.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <WebSocketsClient.h>

#include <Wire.h>
#include <LiquidCrystal.h>

#include <algorithm>

#define json_byte 256

LiquidCrystal lcd_keypad(0, 2, 4, 14, 12, 13);

ESP8266WiFiMulti wifi;
WebSocketsClient ws_client;

namespace utils
{
  std::string random_message_id()
  {
    auto randchar = []() -> char {
      const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
      const std::size_t max_index = (sizeof(charset) - 1);
      return charset[rand() % max_index];
    };

    std::string str(4, 0);
    std::generate_n(str.begin(), 4, randchar);

    return str;
  }

  int maxmin(float n, float lower, float upper)
  {
    return std::max(lower, std::min(n, upper));
  }
}; // namespace utils

namespace values
{
  std::vector<std::string> scene_list = {};

  static bool limit = false;
  static bool take = false;
  int selected_scene = 0;

  String current_scene = "";
  String old_scene = "";

  // Wifi Networks example
  const char *ssid01 = "Loading…";
  const char *ssid02 = "Free Wi-Fi for everyone";
  const char *ssid03 = "Wi-Fi is permanent";
  const char *ssid04 = "Trade Bandwidth for Beer";

  // Wifi Passwords example
  const char *password01 = "ZJ8K7T6tpz4dGngX";
  const char *password = "38fWsVMC546C27Qe";

  // OBS Address example
  const char *ws_address = "192.168.10.125";
}; // namespace values

namespace requests
{
  inline void scene(String scene_name)
  {
    String request = "";

    StaticJsonDocument<json_byte> doc;
    doc["scene-name"] = scene_name;
    doc["message-id"] = utils::random_message_id().c_str();
    doc["request-type"] = "SetCurrentScene";
    serializeJson(doc, request);

    // send request
    ws_client.sendTXT(request);

    // clear alloc memory
    doc.clear();
  }

  inline void toggle_streaming()
  {
    String request = "";

    StaticJsonDocument<json_byte> doc;
    doc["message-id"] = utils::random_message_id().c_str();
    doc["request-type"] = "StartStopStreaming";
    serializeJson(doc, request);

    // send request
    ws_client.sendTXT(request);

    // clear alloc memory
    doc.clear();
  }

  inline void scene_list()
  {
    String request = "";

    StaticJsonDocument<json_byte> doc;
    doc["message-id"] = utils::random_message_id().c_str();
    doc["request-type"] = "GetSceneList";
    serializeJson(doc, request);

    // send request
    ws_client.sendTXT(request);

    // clear alloc memory
    doc.clear();
  }

  inline void run()
  {
    // no esp8266 cpu stress
    if (millis() % 50 != 0)
      return;

    // streaming button
    if (analogRead(A0) < 100)
    {
      delay(500);
      requests::toggle_streaming();
    }

    // select scene in list
    else if (analogRead(A0) < 300)
    {
      delay(500);
      values::take = false;

      if (values::scene_list.size() > 0)
      {
        if (values::limit == false)
          values::selected_scene += 1;
        else
          values::selected_scene -= 1;

        if (values::selected_scene == static_cast<int>(values::scene_list.size()))
          values::limit = true;
        else if (values::selected_scene == 1)
          values::limit = false;

        values::selected_scene = utils::maxmin(values::selected_scene, 1, values::scene_list.size());

        lcd_keypad.clear();
        lcd_keypad.setCursor(0, 0);
        lcd_keypad.print("Selected scene:");
        lcd_keypad.setCursor(1, 1);
        lcd_keypad.print(values::scene_list.at(values::selected_scene - 1).c_str());
      }
    }

    // set selected scene
    else if (analogRead(A0) < 500)
    {
      delay(500);

      if (values::take == true)
      {
        if (values::old_scene != "null" && !values::old_scene.isEmpty())
          requests::scene(values::old_scene);
      }
      else
      {
        requests::scene(values::scene_list.at(values::selected_scene - 1).c_str());
        values::take = true;
      }
    }
  }
}; // namespace requests

namespace callbacks
{
  inline void exiting()
  {
    lcd_keypad.clear();
    lcd_keypad.printf("OBS left!");
    ws_client.disconnect();
  }

  inline void switch_scenes(String scene_name)
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Active scene:");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print(scene_name);
  }

  inline void stream_starting()
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Streaming");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print("starting...");
  }

  inline void stream_started()
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Streaming");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print("started!");
  }

  inline void stream_stopping()
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Streaming");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print("stopping...");
  }

  inline void stream_stopped()
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Streaming");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print("stopped!");
  }

  inline void stream_status(String stream_time)
  {
    lcd_keypad.clear();
    lcd_keypad.setCursor(0, 0);
    lcd_keypad.print("Stream time:");
    lcd_keypad.setCursor(1, 1);
    lcd_keypad.print(stream_time);
  }

  inline void transition_begin(String current_scene, String old_scene)
  {
    values::current_scene = current_scene;
    values::old_scene = old_scene;
  }

  inline void run(String type, JsonObject jopayload)
  {
    if (type.equals("SwitchScenes"))
    {
      String result = jopayload["scene-name"];
      callbacks::switch_scenes(result);
    }
    else if (type.equals("Exiting"))
      callbacks::exiting();
    else if (type.equals("StreamStarting"))
      callbacks::stream_starting();
    else if (type.equals("StreamStarted"))
      callbacks::stream_started();
    else if (type.equals("StreamStopping"))
      callbacks::stream_stopping();
    else if (type.equals("StreamStopped"))
      callbacks::stream_stopped();
    else if (type.equals("StreamStatus"))
    {
      String result = jopayload["stream-timecode"];
      callbacks::stream_status(result);
    }
    else if (type.equals("TransitionBegin"))
    {
      String current_scene = jopayload["to-scene"];
      String old_scene = jopayload["from-scene"];
      callbacks::transition_begin(current_scene, old_scene);
    }
  }

  inline void run_scene_list(JsonObject jopayload)
  {
    String list = jopayload["scenes"];
    std::string converted = list.c_str();
    std::string name = "name";
    std::size_t count = 0, pos = 0;

    while ((pos = converted.find(name, pos)) != std::string::npos)
    {
      pos += name.size();
      count++;
      yield();
    }

    for (int i = 0; i < static_cast<int>(count); i++)
    {
      String scene = jopayload["scenes"][i]["name"];
      std::string converted = scene.c_str();

      if (converted != "null" && !converted.empty())
        values::scene_list.emplace_back(converted);

      yield();
    }

    String current_scene = jopayload["current-scene"];
    values::current_scene = current_scene;
  }
}; // namespace callbacks

void ws_event(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
  {
    delay(2000);

    lcd_keypad.clear();
    lcd_keypad.printf("WS disconnected!");
    break;
  }
  case WStype_CONNECTED:
  {
    lcd_keypad.clear();
    lcd_keypad.printf("WS connected!");

    // increase use
    requests::scene_list();
    delay(2000);

    lcd_keypad.clear();
    lcd_keypad.printf("Send requests!");
    break;
  }
  case WStype_TEXT:
  {
    StaticJsonDocument<JSON_OBJECT_SIZE(json_byte)> doc;
    deserializeJson(doc, payload);
    JsonObject object = doc.as<JsonObject>();

    if (!object["update-type"].isNull())
    {
      String type = object["update-type"];
      callbacks::run(type, object);
    }

    if (!object["scenes"].isNull())
    {
      callbacks::run_scene_list(object);
    }

    // clear alloc memory
    doc.clear();
    object.clear();
    break;
  }
  default:
    break;
  }
}

void setup()
{
  // init lcd
  lcd_keypad.begin(20, 2);

  // init button
  pinMode(A0, INPUT_PULLUP);

  // disconnect previous wifi connection
  WiFi.disconnect();

  // wifi connect
  wifi.addAP(values::ssid01, values::password01);
  wifi.addAP(values::ssid02, values::password);
  wifi.addAP(values::ssid03, values::password);
  wifi.addAP(values::ssid04, values::password);

  lcd_keypad.clear();
  lcd_keypad.setCursor(0, 0);
  lcd_keypad.printf("Connecting to");
  lcd_keypad.setCursor(1, 1);
  lcd_keypad.printf("wifi");
  while (wifi.run() != WL_CONNECTED)
  {
    delay(500);
    lcd_keypad.printf(".");
  }

  if (wifi.run() == WL_CONNECTED)
  {
    lcd_keypad.clear();
    lcd_keypad.printf("Wifi connected!");
  }

  // disconnect previous ws connection
  ws_client.disconnect();

  // init ws connection
  ws_client.begin(values::ws_address, 4444, "/", "arduino");
  ws_client.onEvent(ws_event);
  ws_client.setReconnectInterval(3000);
}

void loop()
{
  ws_client.loop();

  if (wifi.run() == WL_CONNECTED && ws_client.isConnected())
    requests::run();

  yield();
}