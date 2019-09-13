#include "Window.h"

namespace PathTracer
{
    Window::Window(uint32_t width, uint32_t height, bool resizable, std::string const & title)
        : window_width_(width)
        , window_height_(height)
        , window_resizable_(resizable)
        , window_title_(title)
    {
        glfwInit();

        // Create a window with no OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, window_resizable_ ? GLFW_TRUE : GLFW_FALSE);
        window_ = glfwCreateWindow(window_width_, window_height_, window_title_.c_str(), nullptr, nullptr);
    }
    Window::Window()
    {
        glfwInit();

        // Create a window with no OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, window_resizable_ ? GLFW_TRUE : GLFW_FALSE);
        window_ = glfwCreateWindow(window_width_, window_height_, window_title_.c_str(), nullptr, nullptr);
    }
    Window::~Window()
    {
        if (!window_) return;
        // Tear down the window
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}