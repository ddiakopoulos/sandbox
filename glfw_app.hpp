#pragma once

#ifndef glfw_app_h
#define glfw_app_h

#include "string_utils.hpp"
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "geometric.hpp"
#include "util.hpp"

#include <thread>
#include <chrono>
#include <codecvt>
#include <string>

#if defined(ANVIL_PLATFORM_WINDOWS)
#define GLEW_STATIC
#include "glew.h"
#endif

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

namespace util
{

struct UpdateEvent
{
    double elapsed_s;
    float timestep_ms;
    float framesPerSecond;
};

struct InputEvent
{
    enum Type { CURSOR, MOUSE, KEY, CHAR, SCROLL };
    
    GLFWwindow * window;
    math::int2 windowSize;
    
    Type type;
    int action;
    int mods;
    
    math::float2 cursor;
    bool drag = false;
    
    math::uint2 value; // button, key, codepoint, scrollX, scrollY
    
    bool is_mouse_down() const { return action != GLFW_RELEASE; }
    bool is_mouse_up() const { return action == GLFW_RELEASE; }
    
    bool using_shift_key() const { return mods & GLFW_MOD_SHIFT; };
    bool using_control_key() const { return mods & GLFW_MOD_CONTROL; };
    bool using_alt_key() const { return mods & GLFW_MOD_ALT; };
    bool using_super_key() const { return mods & GLFW_MOD_SUPER; };
};
    
class GLFWApp
{
public:

    GLFWApp(int w, int h, const std::string windowTitle, int glfwSamples = 2);
    virtual ~GLFWApp();

    void main_loop();

    virtual void on_update(const UpdateEvent & e) {}
    virtual void on_draw() {}
    virtual void on_window_focus(bool focused) {}
    virtual void on_window_resize(math::int2 size) {}
    virtual void on_input(const InputEvent & event) {}
    virtual void on_drop(std::vector<std::string> names) {}
    virtual void on_uncaught_exception(std::exception_ptr e);

    math::float2 get_cursor_position() const;

    void exit();

    void set_fullscreen(bool state);
    bool get_fullscreen();

protected:

    GLFWwindow * window;

private:
    
    bool isDragging = false;
    
    void consume_character(uint32_t codepoint);
    void consume_key(int key, int action);
    void consume_mousebtn(int button, int action);
    void consume_cursor(double xpos, double ypos);
    void consume_scroll(double xoffset, double yoffset);
    
    void preprocess_input(InputEvent & event);

    static void enter_fullscreen(GLFWwindow * window, math::int2 & windowedSize, math::int2 & windowedPos);
    static void exit_fullscreen(GLFWwindow * window, const math::int2 & windowedSize, const math::int2 & windowedPos);

    uint64_t elapsedFrames = 0;
    uint64_t fps = 0;
    double fpsTime = 0;
    int lastButton = 0;
    bool fullscreenState = false; 

    math::int2 windowedSize; 
    math::int2 windowedPos;

    std::vector<std::exception_ptr> exceptions;
};
    
extern int Main(int argc, char * argv[]);
    
} // end namespace util

#endif

#define IMPLEMENT_MAIN(...) namespace util { int main(int argc, char * argv[]); } int main(int argc, char * argv[]) { return util::Main(argc, argv); } int util::Main(__VA_ARGS__)
