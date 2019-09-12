#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <vulkan/vulkan.h>
#include "SceneController.h"
#include "AccelerationStructureController.h"
#include "VulkanManager.h"

namespace PathTracer
{
    // geometry parameters
    struct Shape
    {
        uint32_t count_;
        uint32_t first_index_;
        uint32_t base_vertex_;
        uint32_t padding_;
    };

    class PathTracerImpl
    {
    public:
        PathTracerImpl() = default;
        void Init(Scene const& scene, RrAccelerationStructure top_level_structure, std::shared_ptr<VulkanManager> manager);
        CommandBuffer PreparePathTraceCommands();

    private:
        std::shared_ptr<VulkanManager> manager_ = nullptr;
        RrAccelerationStructure top_level_structure_ = VK_NULL_HANDLE;
        VkBuffer indices_ = VK_NULL_HANDLE;
        VkBuffer vertices_ = VK_NULL_HANDLE;
        VkBuffer shapes_ = VK_NULL_HANDLE;
        VkBuffer color_ = VK_NULL_HANDLE;
        VkBuffer params_ = VK_NULL_HANDLE;
        VkBuffer scratch_trace_ = VK_NULL_HANDLE;
        VkBuffer camera_rays_ = VK_NULL_HANDLE;
        VkBuffer ao_rays_ = VK_NULL_HANDLE;
        VkBuffer ao_count_ = VK_NULL_HANDLE;
        VkBuffer hits_ = VK_NULL_HANDLE;
        VkBuffer random_ = VK_NULL_HANDLE;
        VkBuffer ao_ = VK_NULL_HANDLE;
        VkBuffer ao_id_ = VK_NULL_HANDLE;
    };
}