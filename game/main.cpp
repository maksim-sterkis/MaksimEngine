#include "engine.hpp"
#include "model.hpp"
#include <imgui.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace vke;

struct PushConstants {
  glm::mat4 mvp;
  float colorOverride[4];
  int useOverride;
};

int triangle_color_idx = 0;
bool triangle_follow_mouse = false;
float triangle_pos[2] = {0.0f, 0.0f};

int current_view = 1; // 1 = Triangle, 2 = Cube
float camera_distance = 3.0f;

std::unique_ptr<vke::Model> cubeModel;
std::unique_ptr<vke::Model> triangleModel;

bool point_in_triangle(float px, float py, float x1, float y1, float x2, float y2, float x3, float y3) {
    auto sign = [](float p1x, float p1y, float p2x, float p2y, float p3x, float p3y) {
        return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
    };
    bool b1 = sign(px, py, x1, y1, x2, y2) <= 0.0f;
    bool b2 = sign(px, py, x2, y2, x3, y3) <= 0.0f;
    bool b3 = sign(px, py, x3, y3, x1, y1) <= 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

void update_game(EngineState &state, float dt) {
  // Handle Input
  static bool prev_space = false;
  static bool prev_mouse_left = false;
  static bool prev_key_1 = false;
  static bool prev_key_2 = false;
  
  bool curr_space = input::is_key_pressed(state.input, GLFW_KEY_SPACE);
  bool curr_mouse_left = input::is_mouse_pressed(state.input, GLFW_MOUSE_BUTTON_LEFT);
  bool curr_key_1 = input::is_key_pressed(state.input, GLFW_KEY_1);
  bool curr_key_2 = input::is_key_pressed(state.input, GLFW_KEY_2);

  double mx, my;
  input::get_mouse_pos(state.input, mx, my);
  
  float winW = static_cast<float>(state.window.width);
  float winH = static_cast<float>(state.window.height);
  float winAspect = winW > 0.0f && winH > 0.0f ? winW / winH : 1.0f;
  float targetAspect = static_cast<float>(state.baseWidth) / static_cast<float>(state.baseHeight);
  float scaleX = 1.0f;
  float scaleY = 1.0f;

  float vpW = winW;
  float vpH = winH;
  float vpX = 0.0f;
  float vpY = 0.0f;

  if (state.aspectMode == AspectMode::ULTRAWIDE) {
    if (winAspect > targetAspect) scaleX = targetAspect / winAspect;
    else scaleY = winAspect / targetAspect;
  } else if (state.aspectMode == AspectMode::FIXED) {
    if (winAspect > targetAspect) {
      vpW = winH * targetAspect;
      vpX = (winW - vpW) / 2.0f;
    } else {
      vpH = winW / targetAspect;
      vpY = (winH - vpH) / 2.0f;
    }
  }

  // Convert mouse to viewport NDC
  float ndc_x = vpW > 0.0f ? ((static_cast<float>(mx) - vpX) / vpW) * 2.0f - 1.0f : 0.0f;
  float ndc_y = vpH > 0.0f ? ((static_cast<float>(my) - vpY) / vpH) * 2.0f - 1.0f : 0.0f;
  
  bool over_ui = ImGui::GetIO().WantCaptureMouse;

  if (curr_key_1 && !prev_key_1) current_view = 1;
  if (curr_key_2 && !prev_key_2) current_view = 2;

  if (current_view == 2) {
    double scroll = input::get_scroll(state.input);
    camera_distance -= scroll * 0.5f;
    if (camera_distance < 1.0f) camera_distance = 1.0f;
    if (camera_distance > 15.0f) camera_distance = 15.0f;
  }

  if (curr_space && !prev_space) {
      triangle_follow_mouse = !triangle_follow_mouse;
  }
  
  if (curr_mouse_left && !prev_mouse_left && !over_ui && current_view == 1) {
      float x1 = triangle_pos[0] + 0.0f * scaleX;
      float y1 = triangle_pos[1] - 0.5f * scaleY;
      float x2 = triangle_pos[0] + 0.5f * scaleX;
      float y2 = triangle_pos[1] + 0.5f * scaleY;
      float x3 = triangle_pos[0] - 0.5f * scaleX;
      float y3 = triangle_pos[1] + 0.5f * scaleY;
      if (point_in_triangle(ndc_x, ndc_y, x1, y1, x2, y2, x3, y3)) {
          triangle_color_idx = (triangle_color_idx + 1) % 4;
      }
  }
  
  if (triangle_follow_mouse && current_view == 1) {
      triangle_pos[0] = ndc_x;
      triangle_pos[1] = ndc_y;
  }

  prev_space = curr_space;
  prev_mouse_left = curr_mouse_left;
  prev_key_1 = curr_key_1;
  prev_key_2 = curr_key_2;

  static bool show_ui = false;
  if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
    show_ui = !show_ui;
  }

  if (show_ui) {
    ImGui::Begin("Engine Debug");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Mode: %s (Press 1 or 2 to switch)", current_view == 1 ? "Triangle" : "Cube");
    ImGui::Text("Mouse: (%.1f, %.1f)", mx, my);
    ImGui::Separator();
    int mode = static_cast<int>(state.aspectMode);
    ImGui::Text("Aspect Ratio Mode:");
    ImGui::RadioButton("Ultrawide (Dynamic)", &mode,
                       static_cast<int>(AspectMode::ULTRAWIDE));
    ImGui::RadioButton("Fixed (Black Bars)", &mode,
                       static_cast<int>(AspectMode::FIXED));
    state.aspectMode = static_cast<AspectMode>(mode);

    ImGui::Separator();
    int window_mode = static_cast<int>(state.window.currentMode);
    ImGui::Text("Window Mode:");
    if (ImGui::RadioButton("Windowed", &window_mode,
                           static_cast<int>(WindowMode::WINDOWED))) {
      state.swapchain.disallowExclusive = true;
      window::set_mode(state.window, WindowMode::WINDOWED);
      glfwPollEvents();
      engine::recreate_swapchain(state);
    }
    if (ImGui::RadioButton("Borderless", &window_mode,
                           static_cast<int>(WindowMode::BORDERLESS))) {
      state.swapchain.disallowExclusive = true;
      window::set_mode(state.window, WindowMode::BORDERLESS);
      glfwPollEvents();
      engine::recreate_swapchain(state);
    }
    if (ImGui::RadioButton("Fullscreen (Exclusive)", &window_mode,
                           static_cast<int>(WindowMode::FULLSCREEN))) {
      state.swapchain.disallowExclusive = false;
      window::set_mode(state.window, WindowMode::FULLSCREEN);
      state.swapchain.vsync = true;
      glfwPollEvents();
      engine::recreate_swapchain(state);
    }

    if (state.window.currentMode != WindowMode::FULLSCREEN) {
      bool oldVsync = state.swapchain.vsync;
      if (ImGui::Checkbox("Vsync (FIFO)", &state.swapchain.vsync)) {
        if (oldVsync != state.swapchain.vsync) {
          engine::recreate_swapchain(state);
        }
      }
    }

    ImGui::End();
  }

  static bool show_exit_prompt = false;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    show_exit_prompt = !show_exit_prompt;
  }

  if (show_exit_prompt) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Exit Prompt", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    ImGui::Text("Are you sure you want to exit?");
    ImGui::Spacing();

    if (ImGui::Button("Exit Game", ImVec2(120, 40))) {
      glfwSetWindowShouldClose(state.window.handle, GLFW_TRUE);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 40))) {
      show_exit_prompt = false;
    }
    ImGui::End();
  }
}

void draw_scene(EngineState &state, VkCommandBuffer cmd) {
  float winW = static_cast<float>(state.swapchain.extent.width);
  float winH = static_cast<float>(state.swapchain.extent.height);
  float winAspect = winW > 0.0f && winH > 0.0f ? winW / winH : 1.0f;
  float targetAspect = static_cast<float>(state.baseWidth) /
                       static_cast<float>(state.baseHeight);

  PushConstants push{};
  
  if (current_view == 1 && triangleModel) {
      glm::mat4 ortho = glm::mat4(1.0f);
      if (state.aspectMode == AspectMode::ULTRAWIDE) {
          float scaleX = winAspect > targetAspect ? targetAspect / winAspect : 1.0f;
          float scaleY = winAspect > targetAspect ? 1.0f : winAspect / targetAspect;
          ortho = glm::scale(glm::mat4(1.0f), glm::vec3(scaleX, scaleY, 1.0f));
      }

      glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(triangle_pos[0], triangle_pos[1], 0.0f));
      push.mvp = ortho * model;
      push.useOverride = 1;

      if (triangle_color_idx == 0) { // Multi
          push.useOverride = 0;
      } else if (triangle_color_idx == 1) { // Green
          push.colorOverride[0] = 0.0f; push.colorOverride[1] = 1.0f; push.colorOverride[2] = 0.0f; push.colorOverride[3] = 1.0f;
      } else if (triangle_color_idx == 2) { // Blue
          push.colorOverride[0] = 0.0f; push.colorOverride[1] = 0.0f; push.colorOverride[2] = 1.0f; push.colorOverride[3] = 1.0f;
      } else { // Red
          push.colorOverride[0] = 1.0f; push.colorOverride[1] = 0.0f; push.colorOverride[2] = 0.0f; push.colorOverride[3] = 1.0f;
      }

      vkCmdPushConstants(cmd, state.pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(PushConstants), &push);
      
      triangleModel->bind(cmd);
      triangleModel->draw(cmd);
  } else if (current_view == 2 && cubeModel) {
      glm::mat4 proj;
      if (state.aspectMode == AspectMode::FIXED) {
          proj = glm::perspective(glm::radians(45.0f), targetAspect, 0.1f, 100.0f);
      } else {
          proj = glm::perspective(glm::radians(45.0f), winAspect, 0.1f, 100.0f);
      }
      proj[1][1] *= -1; // Vulkan Y-flip
      
      glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, camera_distance), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 model = glm::rotate(glm::mat4(1.0f), static_cast<float>(glfwGetTime()), glm::vec3(1.0f, 1.0f, 0.0f));

      push.mvp = proj * view * model;
      push.useOverride = 0;

      vkCmdPushConstants(cmd, state.pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(PushConstants), &push);
      
      cubeModel->bind(cmd);
      cubeModel->draw(cmd);
  }
}

int main() {
  EngineState engineState;

  EngineConfig config{};
  config.windowTitle = "VK_game_engine - 3D Cube";
  config.windowWidth = 800;
  config.windowHeight = 600;
  config.vertexShaderPath = "shaders/shader.vert.spv";
  config.fragmentShaderPath = "shaders/shader.frag.spv";
  config.clearColor[0] = 0.01f;
  config.clearColor[1] = 0.01f;
  config.clearColor[2] = 0.01f;
  config.clearColor[3] = 1.0f;

  try {
    engine::init(engineState, config);
    
    // Init Triangle Model
    std::vector<vke::Vertex> triangleVertices = {
        {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}
    };
    triangleModel = std::make_unique<vke::Model>(engineState.device, triangleVertices);
    
    // Init Cube Model
    try {
        cubeModel = vke::Model::loadOBJ(engineState.device, "assets/obj/cube.obj");
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not load cube model: " << e.what() << "\n";
    }

    engine::run(engineState, update_game, draw_scene);
    
    // Reset models before device goes away
    triangleModel.reset();
    cubeModel.reset();

    engine::cleanup(engineState);
  } catch (const std::exception &e) {
    std::cerr << "Fatal Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
