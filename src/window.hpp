#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace vke {

enum class WindowMode { WINDOWED, BORDERLESS, FULLSCREEN };

struct WindowState {
  GLFWwindow *handle = nullptr;
  int width = 0;
  int height = 0;
  std::string name;
  bool framebufferResized = false;

  WindowMode currentMode = WindowMode::WINDOWED;
  int windowedX = 100;
  int windowedY = 100;
  int windowedWidth = 800;
  int windowedHeight = 600;
};

namespace window {

void create(WindowState &state, int w, int h, const char *name);
void set_mode(WindowState &state, WindowMode mode);
void destroy(WindowState &state);
bool should_close(const WindowState &state);
bool was_window_resized(const WindowState &state);
void reset_window_resized_flag(WindowState &state);
VkExtent2D get_extent(const WindowState &state);
void create_surface(const WindowState &state, VkInstance instance,
                    VkSurfaceKHR *surface);

} // namespace window
} // namespace vke
