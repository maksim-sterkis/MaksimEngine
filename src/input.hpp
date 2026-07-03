#pragma once

#include "Window.hpp"
#include <unordered_map>

namespace vke {

struct InputState {
  std::unordered_map<int, bool> keys;
  double mouseX = 0.0;
  double mouseY = 0.0;
  bool mouseLeft = false;
  bool mouseRight = false;
  bool mouseMiddle = false;
  double scrollDirection = 0.0;
  double lastScrollTime = 0.0;
};

namespace input {

void init(InputState &state, WindowState &window);
bool is_key_pressed(const InputState &state, int key);
bool is_mouse_pressed(const InputState &state, int button);
void get_mouse_pos(const InputState &state, double &x, double &y);
double get_scroll(InputState &state);
const char *get_key_name(int key);

} // namespace input
} // namespace vke
