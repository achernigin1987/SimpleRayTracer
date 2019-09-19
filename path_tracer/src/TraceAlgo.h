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
    struct Shape
    {
        uint32_t count_;
        uint32_t first_index_;
        uint32_t base_vertex_;
        uint32_t material_id;
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

    class TraceAlgo
    {
    public:
        virtual ~TraceAlgo() = default;
        virtual void Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays) = 0;
        virtual VkResult Submit() = 0;
        virtual void UpdateView(Params const& params) = 0;
        virtual std::vector<uint32_t> GetColor() const = 0;
        virtual VkBuffer GetColorBuffer() const = 0;
        virtual void SetColor(std::vector<uint32_t> const&) = 0;
    };
}