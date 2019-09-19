#pragma once

#include "TraceAlgo.h"

namespace PathTracer
{
    class PathTrace : public TraceAlgo
    {
    public:
        PathTrace(std::shared_ptr<VulkanManager> manager);
        ~PathTrace();
        void Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays) override;
        VkResult Submit() override;
        void UpdateView(Params const& params) override;
        VkBuffer GetColorBuffer() const override;

    private:
        void PrepareCommandBuffer(uint32_t num_rays);

        std::shared_ptr<VulkanManager> manager_ = nullptr;
        RrAccelerationStructure top_level_structure_ = VK_NULL_HANDLE;
        RrContext context_ = VK_NULL_HANDLE;
        struct PathTraceImpl;
        std::unique_ptr<PathTraceImpl> impl_;
    };

}