#ifndef JSON_CONVERTERS_H
#define JSON_CONVERTERS_H

#include <ArduinoJson.h>
#include "MacroHandler.h" // Include the existing macro handler

// Custom converter for std::vector<MacroCommand>
namespace ArduinoJson {
  template <>
  struct Converter<std::vector<MacroCommand>> {
    static bool toJson(const std::vector<MacroCommand>& src, JsonVariant dst) {
      JsonArray array = dst.to<JsonArray>();
      for (const auto& cmd : src) {
        JsonObject obj = array.createNestedObject();
        
        // Set the command type based on the enum
        switch (cmd.type) {
          case MACRO_CMD_KEY_PRESS:
            obj["type"] = "key_press";
            {
              JsonArray report = obj.createNestedArray("report");
              for (int i = 0; i < 8; i++) {
                report.add(String("0x") + String(cmd.data.keyPress.report[i], HEX));
              }
            }
            break;
          case MACRO_CMD_KEY_DOWN:
            obj["type"] = "key_press";
            {
              JsonArray report = obj.createNestedArray("report");
              for (int i = 0; i < 8; i++) {
                report.add(String("0x") + String(cmd.data.keyPress.report[i], HEX));
              }
            }
            break;
          case MACRO_CMD_KEY_UP:
            obj["type"] = "key_release";
            {
              JsonArray report = obj.createNestedArray("report");
              for (int i = 0; i < 8; i++) {
                report.add(String("0x") + String(cmd.data.keyPress.report[i], HEX));
              }
            }
            break;
          case MACRO_CMD_TYPE_TEXT:
            obj["type"] = "type_text";
            obj["text"] = String(cmd.data.typeText.text, cmd.data.typeText.length);
            break;
          case MACRO_CMD_DELAY:
            obj["type"] = "delay";
            obj["ms"] = cmd.data.delay.milliseconds;
            break;
          case MACRO_CMD_CONSUMER_PRESS:
            obj["type"] = "consumer_press";
            {
              JsonArray report = obj.createNestedArray("report");
              for (int i = 0; i < 4; i++) {
                report.add(String("0x") + String(cmd.data.consumerPress.report[i], HEX));
              }
            }
            break;
          case MACRO_CMD_EXECUTE_MACRO:
            obj["type"] = "execute_macro";
            obj["macroId"] = String(cmd.data.executeMacro.macroId);
            break;
          case MACRO_CMD_MOUSE_MOVE:
            obj["type"] = "mouse_move";
            obj["x"] = cmd.data.mouseMove.x;
            obj["y"] = cmd.data.mouseMove.y;
            break;
          case MACRO_CMD_MOUSE_CLICK:
            obj["type"] = "mouse_button_press";
            obj["button"] = cmd.data.mouseClick.button;
            break;
          case MACRO_CMD_MOUSE_SCROLL:
            obj["type"] = "mouse_wheel";
            obj["amount"] = cmd.data.mouseScroll.amount;
            break;
          default:
            // For any other command types, just set the type
            obj["type"] = "unknown";
            break;
        }
      }
      return true;
    }
  };
}

#endif // JSON_CONVERTERS_H 