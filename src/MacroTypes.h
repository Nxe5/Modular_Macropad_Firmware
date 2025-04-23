#ifndef MACRO_TYPES_H
#define MACRO_TYPES_H

#include <string>
#include <vector>

// Structure for a macro command
struct MacroCommand {
  std::string type;
  
  // Common fields for different command types
  std::vector<std::string> report;  // For key_press, key_release, consumer_press, consumer_release
  int ms;                           // For delay
  std::string text;                 // For type_text
  int x, y;                         // For mouse_move
  int button;                       // For mouse_button_press, mouse_button_release
  int amount;                       // For mouse_wheel
  std::string macroId;              // For execute_macro
};

// Structure for a macro
struct Macro {
  std::string id;
  std::string name;
  std::string description;
  std::vector<MacroCommand> commands;
};

#endif // MACRO_TYPES_H 