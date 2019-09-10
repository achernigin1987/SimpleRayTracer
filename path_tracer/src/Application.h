#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace PathTracer
{
    struct Window
    {
        GLFWwindow*                 window_ = nullptr;
        uint32_t                    window_width_ = 1280;
        uint32_t                    window_height_ = 720;
        bool                        window_resizable_ = false;
        std::string                 window_title_ = "Simple Path Tracer";

        Window(uint32_t width, uint32_t height, bool resizable, std::string const& title)
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

        Window()
        {
            glfwInit();

            // Create a window with no OpenGL context
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, window_resizable_ ? GLFW_TRUE : GLFW_FALSE);
            window_ = glfwCreateWindow(window_width_, window_height_, window_title_.c_str(), nullptr, nullptr);
        }

        ~Window()
        {
            if (!window_) return;
            // Tear down the window
            glfwDestroyWindow(window_);
            glfwTerminate();
        }

        operator GLFWwindow*()
        {
            return window_;
        }
    };

    // Vulkan application
    class Application
    {
        // Non-copyable
        Application(Application const&) = delete;
        Application& operator =(Application const&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

    public:
        Application();
        ~Application();

        // Singleton
        static Application& GetAppInstance()
        {
            static Application s_instance;
            return s_instance;
        }

        int32_t Run(int32_t argc, char const** argv);

    private:

        bool InitVulkan(GLFWwindow* window);
        void TermVulkan();

        // Application interface
        virtual VkResult Init(int32_t argc, char const** argv);
        virtual VkResult Update();
        virtual VkResult Render();
        virtual VkResult Terminate();

        VkSemaphore& SignalSemaphore();
        VkSemaphore& WaitSemaphore();

        struct VulkanManager;

        std::unique_ptr<VulkanManager> vulkan_manager_;
        std::unique_ptr<Window> window_;
    };
}