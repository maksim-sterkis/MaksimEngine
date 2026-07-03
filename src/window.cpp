#include "window.hpp"
#include "engine.hpp"

#include <stdexcept>

namespace vke::window {

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {
  auto engine =
      reinterpret_cast<EngineState *>(glfwGetWindowUserPointer(window));
  if (!engine)
    return;
  engine->window.framebufferResized = true;
  engine->window.width = width;
  engine->window.height = height;
}

void create(WindowState &state, int w, int h, const char *name) {
  state.width = w;
  state.height = h;
  state.name = name;

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  state.handle = glfwCreateWindow(w, h, state.name.c_str(), nullptr, nullptr);
  glfwSetFramebufferSizeCallback(state.handle, framebuffer_resize_callback);
}

void set_mode(WindowState &state, WindowMode mode) {
  if (state.currentMode == mode)
    return;

  if (state.currentMode == WindowMode::WINDOWED) {
    glfwGetWindowPos(state.handle, &state.windowedX, &state.windowedY);
    glfwGetWindowSize(state.handle, &state.windowedWidth,
                      &state.windowedHeight);
  }

  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);

  // If currently fullscreen, we must detach from the monitor before modifying window attributes
  // to prevent GLFW state from getting confused when moving between windowed and exclusive fullscreen.
  if (state.currentMode == WindowMode::FULLSCREEN) {
    glfwSetWindowMonitor(state.handle, nullptr, 0, 0, vidmode->width,
                         vidmode->height, 0);
  }

  if (mode == WindowMode::WINDOWED) {
    glfwSetWindowAttrib(state.handle, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowAttrib(state.handle, GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwSetWindowMonitor(state.handle, nullptr, state.windowedX,
                         state.windowedY, state.windowedWidth,
                         state.windowedHeight, 0);
  } else if (mode == WindowMode::BORDERLESS) {
    glfwSetWindowAttrib(state.handle, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowAttrib(state.handle, GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwSetWindowPos(state.handle, 0, 0);
    glfwSetWindowSize(state.handle, vidmode->width, vidmode->height);
  } else if (mode == WindowMode::FULLSCREEN) {
    glfwSetWindowAttrib(state.handle, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowAttrib(state.handle, GLFW_AUTO_ICONIFY, GLFW_TRUE);
    glfwSetWindowMonitor(state.handle, monitor, 0, 0, vidmode->width,
                         vidmode->height, vidmode->refreshRate);
  }

  state.currentMode = mode;
  state.framebufferResized = true;
}

void destroy(WindowState &state) {
  if (state.handle) {
    glfwDestroyWindow(state.handle);
    state.handle = nullptr;
  }
  glfwTerminate();
}

bool should_close(const WindowState &state) {
  return glfwWindowShouldClose(state.handle);
}

bool was_window_resized(const WindowState &state) {
  return state.framebufferResized;
}

void reset_window_resized_flag(WindowState &state) {
  state.framebufferResized = false;
}

VkExtent2D get_extent(const WindowState &state) {
  return {static_cast<uint32_t>(state.width),
          static_cast<uint32_t>(state.height)};
}

void create_surface(const WindowState &state, VkInstance instance,
                    VkSurfaceKHR *surface) {
  if (glfwCreateWindowSurface(instance, state.handle, nullptr, surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface");
  }
}

} // namespace vke::window
