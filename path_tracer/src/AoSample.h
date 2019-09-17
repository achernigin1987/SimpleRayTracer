#pragma once
#include "TraceAlgo.h"

namespace PathTracer
{
    class Ao : public TraceAlgo
    {
    public:
        Ao(std::shared_ptr<VulkanManager> manager);
        ~Ao();
        void Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays) override;
        VkResult Submit() override;
        void UpdateView(Params const& params) override;
        VkBuffer GetColor() const override;

    private:
        void PrepareCommandBuffer(uint32_t num_rays);

        std::shared_ptr<VulkanManager> manager_ = nullptr;
        RrAccelerationStructure top_level_structure_ = VK_NULL_HANDLE;
        RrContext context_ = VK_NULL_HANDLE;
        struct AoImpl;
        std::unique_ptr<AoImpl> impl_;
    };
}
