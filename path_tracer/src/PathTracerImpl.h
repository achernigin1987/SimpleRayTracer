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
#include "Pipeline.h"

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

    // View parameters
    struct Params
    {
        float eye_[4];
        float center_[4];
        float near_far_[4];
        float screen_dims_[4];
        float view_proj_inv_[16];
    };

    class PathTracerImpl
    {
    public:
        PathTracerImpl(std::shared_ptr<VulkanManager> manager);
        void Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays);
        VkResult Submit();
        void UpdateView(Params const& params);
        VkBuffer GetColor() const
        {
            return color_.get();
        }

    private:
        VkScopedObject<VkFence> CreateFence() const;
        void PrepareCommandBuffer(uint32_t num_rays);

        std::shared_ptr<VulkanManager> manager_ = nullptr;
        RrAccelerationStructure top_level_structure_ = VK_NULL_HANDLE;
        RrContext context_ = VK_NULL_HANDLE;
        VkScopedObject<VkDeviceMemory> memory_;;
        VkScopedObject<VkBuffer> indices_;
        VkScopedObject<VkBuffer> vertices_;
        VkScopedObject<VkBuffer> shapes_;
        VkScopedObject<VkBuffer> color_;
        VkScopedObject<VkBuffer> params_;
        VkScopedObject<VkBuffer> scratch_trace_;
        VkScopedObject<VkBuffer> camera_rays_;
        VkScopedObject<VkBuffer> ao_rays_;
        VkScopedObject<VkBuffer> ao_count_;
        VkScopedObject<VkBuffer> hits_;
        VkScopedObject<VkBuffer> random_;
        VkScopedObject<VkBuffer> ao_;
        VkScopedObject<VkBuffer> ao_id_;
        // The fence to be signalled
        VkScopedObject<VkFence> fence_;
        CommandBuffer ao_command_buffer_;
        // Pipelines
        // The pipeline object for generating the camera rays
        Pipeline camera_rays_pipeline_;
        // The pipeline object for generating the ambient occlusion rays
        Pipeline ao_rays_pipeline_;
        // The pipeline object for resolving the ambient occlusion rays
        Pipeline ao_rays_resolve_pipeline_;
    };
}