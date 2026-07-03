#include "input.hpp"
#include "engine.hpp"

namespace vke::input {

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  auto *engine =
      reinterpret_cast<EngineState *>(glfwGetWindowUserPointer(window));
  if (!engine)
    return;

  if (action == GLFW_PRESS) {
    engine->input.keys[key] = true;
  } else if (action == GLFW_RELEASE) {
    engine->input.keys[key] = false;
  }
}

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  auto *engine =
      reinterpret_cast<EngineState *>(glfwGetWindowUserPointer(window));
  if (!engine)
    return;

  engine->input.mouseX = xpos;
  engine->input.mouseY = ypos;
}

static void scroll_callback(GLFWwindow *window, double xoffset,
                            double yoffset) {
  auto *engine =
      reinterpret_cast<EngineState *>(glfwGetWindowUserPointer(window));
  if (!engine)
    return;

  engine->input.scrollDirection += yoffset;
  engine->input.lastScrollTime = glfwGetTime();
}

static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                  int mods) {
  auto *engine =
      reinterpret_cast<EngineState *>(glfwGetWindowUserPointer(window));
  if (!engine)
    return;

  bool isPressed = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_LEFT)
    engine->input.mouseLeft = isPressed;
  else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    engine->input.mouseRight = isPressed;
  else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    engine->input.mouseMiddle = isPressed;
}

void init(InputState &state, WindowState &window) {
  // Install our callbacks. ImGui will automatically chain them if it was
  // initialized correctly.
  glfwSetKeyCallback(window.handle, key_callback);
  glfwSetCursorPosCallback(window.handle, cursor_position_callback);
  glfwSetScrollCallback(window.handle, scroll_callback);
  glfwSetMouseButtonCallback(window.handle, mouse_button_callback);
}

bool is_key_pressed(const InputState &state, int key) {
  auto it = state.keys.find(key);
  if (it != state.keys.end()) {
    return it->second;
  }
  return false;
}

bool is_mouse_pressed(const InputState &state, int button) {
  if (button == GLFW_MOUSE_BUTTON_LEFT)
    return state.mouseLeft;
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    return state.mouseRight;
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    return state.mouseMiddle;
  return false;
}

void get_mouse_pos(const InputState &state, double &x, double &y) {
  x = state.mouseX;
  y = state.mouseY;
}

double get_scroll(InputState &state) {
  double val = state.scrollDirection;
  state.scrollDirection = 0.0;
  return val;
}

const char *get_key_name(int key) {
  const char *name = glfwGetKeyName(key, 0);
  if (name != nullptr) {
    return name;
  }

  switch (key) {
  case GLFW_KEY_SPACE:
    return "Space";
  case GLFW_KEY_ESCAPE:
    return "Escape";
  case GLFW_KEY_ENTER:
    return "Enter";
  case GLFW_KEY_TAB:
    return "Tab";
  case GLFW_KEY_BACKSPACE:
    return "Backspace";
  case GLFW_KEY_INSERT:
    return "Insert";
  case GLFW_KEY_DELETE:
    return "Delete";
  case GLFW_KEY_RIGHT:
    return "Right Arrow";
  case GLFW_KEY_LEFT:
    return "Left Arrow";
  case GLFW_KEY_DOWN:
    return "Down Arrow";
  case GLFW_KEY_UP:
    return "Up Arrow";
  case GLFW_KEY_PAGE_UP:
    return "Page Up";
  case GLFW_KEY_PAGE_DOWN:
    return "Page Down";
  case GLFW_KEY_HOME:
    return "Home";
  case GLFW_KEY_END:
    return "End";
  case GLFW_KEY_CAPS_LOCK:
    return "Caps Lock";
  case GLFW_KEY_SCROLL_LOCK:
    return "Scroll Lock";
  case GLFW_KEY_NUM_LOCK:
    return "Num Lock";
  case GLFW_KEY_PRINT_SCREEN:
    return "Print Screen";
  case GLFW_KEY_PAUSE:
    return "Pause";
  case GLFW_KEY_F1:
    return "F1";
  case GLFW_KEY_F2:
    return "F2";
  case GLFW_KEY_F3:
    return "F3";
  case GLFW_KEY_F4:
    return "F4";
  case GLFW_KEY_F5:
    return "F5";
  case GLFW_KEY_F6:
    return "F6";
  case GLFW_KEY_F7:
    return "F7";
  case GLFW_KEY_F8:
    return "F8";
  case GLFW_KEY_F9:
    return "F9";
  case GLFW_KEY_F10:
    return "F10";
  case GLFW_KEY_F11:
    return "F11";
  case GLFW_KEY_F12:
    return "F12";
  case GLFW_KEY_LEFT_SHIFT:
    return "Left Shift";
  case GLFW_KEY_LEFT_CONTROL:
    return "Left Control";
  case GLFW_KEY_LEFT_ALT:
    return "Left Alt";
  case GLFW_KEY_LEFT_SUPER:
    return "Left Super";
  case GLFW_KEY_RIGHT_SHIFT:
    return "Right Shift";
  case GLFW_KEY_RIGHT_CONTROL:
    return "Right Control";
  case GLFW_KEY_RIGHT_ALT:
    return "Right Alt";
  case GLFW_KEY_RIGHT_SUPER:
    return "Right Super";
  case GLFW_KEY_MENU:
    return "Menu";
  default:
    return "Unknown";
  }
}

} // namespace vke::input
