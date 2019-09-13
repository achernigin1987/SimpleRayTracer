#pragma once
#include <GLFW/glfw3.h>
#include <string>

namespace PathTracer
{
    struct Window
    {
        GLFWwindow*                 window_ = nullptr;
        uint32_t                    window_width_ = 1280;
        uint32_t                    window_height_ = 720;
        bool                        window_resizable_ = false;
        std::string                 window_title_ = "Simple Path Tracer";

        Window(uint32_t width, uint32_t height, bool resizable, std::string const& title);

        Window();

        ~Window();

        operator GLFWwindow*() const
        {
            return window_;
        }
    };
}