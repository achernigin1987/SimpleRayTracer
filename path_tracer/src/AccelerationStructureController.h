#pragma once

#include <radeonrays.h>
#include <vector>
#include "SceneController.h"
#include "Application.h"

namespace PathTracer
{
    class Application::AccelerationStructureController
    {
    public:
        AccelerationStructureController(std::shared_ptr<VulkanManager> vk_manager);
        ~AccelerationStructureController();

        // Non-copyable
        AccelerationStructureController(AccelerationStructureController const&) = delete;
        AccelerationStructureController& operator =(AccelerationStructureController const&) = delete;
        AccelerationStructureController(AccelerationStructureController&&) = delete;
        AccelerationStructureController& operator=(AccelerationStructureController&&) = delete;

        // building
        bool BuildAccelerationStructure(Scene const& scene);
        inline RrAccelerationStructure Get() const
        {
            return top_level_accel_;
        }
    private:
        // vulkan manager for context, memory management and scheduling build
        std::shared_ptr<VulkanManager> vulkan_manager_;
        // The RadeonRays context
        RrContext context_;
        // The RadeonRays acceleration structures
        std::vector<RrAccelerationStructure> bottom_level_accel_;
        RrAccelerationStructure top_level_accel_;
        // The buffer for storing the acceleration structure
        // only one since we have single memory "ptr" covering all acceleration structures
        VkDeviceMemory accel_buffer_;
    };
}
