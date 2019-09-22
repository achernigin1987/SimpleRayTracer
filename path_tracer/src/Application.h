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

#include "Window.h"
#include "Orbit.h"
#include "VulkanManager.h"

#include "AoSample.h"
#include "AccelerationStructureController.h"
#include "InferenceEngine.h"

namespace PathTracer
{
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
        // Application interface
        VkResult Init(int32_t argc, char const** argv);
        VkResult Update();
        VkResult Render();
        VkResult InitCallbacks();

        void OnMouseScroll(float scroll);
        void OnMouseMove(glm::vec2 const& position);
        void OnMousePress(int32_t button, int32_t action, int32_t modifiers);

        VkSemaphore& SignalSemaphore();
        VkSemaphore& WaitSemaphore();
        VkResult SubmitBlitCommandBuffer(std::vector<CommandBuffer> const& blit_command_buffers, VkQueue queue, VkFence fence = VK_NULL_HANDLE);

        std::shared_ptr<VulkanManager> vulkan_manager_;
        std::unique_ptr<AccelerationStructureController> as_controller_;
        std::unique_ptr<Window> window_;
        std::unique_ptr<InferenceEngine> engine_;
        // The camera controller
        Orbit orbit_;
        // The view-projection matrix
        glm::mat4 view_projection_;
        // The number of samples
        uint32_t sample_count_;
        std::unique_ptr <TraceAlgo> trace_algo_;
        std::vector<CommandBuffer> blit_cmd_buffers_;
    };
}