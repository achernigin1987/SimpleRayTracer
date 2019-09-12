#include "PathTracerImpl.h"

namespace PathTracer
{
    void PathTracerImpl::Init(Scene const & scene, RrAccelerationStructure top_level_structure, std::shared_ptr<VulkanManager> manager)
    {
        top_level_structure_ = top_level_structure;
        manager_ = manager;



    }
}