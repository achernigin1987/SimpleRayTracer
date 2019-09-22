#include "MBPathTracing.h"
#include <random>
#include <glm/glm.hpp>

namespace PathTracer
{

    struct MBPathTrace::MBPathTraceImpl
    {
        // vulkan manager
        std::shared_ptr<VulkanManager> manager_;
        // ao buffers
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> indices_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> vertices_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> shapes_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> color_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> params_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> scratch_trace_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> camera_rays_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> ao_rays_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> ao_count_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> hits_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> shadow_hits_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> random_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> ao_;
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> ao_id_;

        // color size for mapping
        VkDeviceSize color_size_;

        // textures
        std::vector<VkDescriptorImageInfo> image_infos_;
        std::vector<VkScopedObject<VkImage>> texture_image_;
        std::vector<VkScopedObject<VkDeviceMemory>> texture_image_memory_;
        std::vector<VkScopedObject<VkImageView>> texture_image_view_;
        std::vector<VkScopedObject<VkSampler>> texture_sampler_;

        // The fence to be signalled
        VkScopedObject<VkFence> fence_;
        // pipelines
        CommandBuffer pt_command_buffer_;
        // Pipelines
        // The pipeline object for generating the camera rays
        Pipeline camera_rays_pipeline_;
        // The pipeline object for shading
        Pipeline rt_shade_pipeline_;
        // The pipeline object for shading
        Pipeline clear_counter_pipeline_;
        // The pipeline object for generating the ambient occlusion rays
        Pipeline ao_rays_pipeline_;
        // The pipeline object for resolving the ambient occlusion rays
        Pipeline ao_rays_resolve_pipeline_;
    };

}