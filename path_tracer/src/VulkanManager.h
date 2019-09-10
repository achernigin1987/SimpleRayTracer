#pragma once
#include "Application.h"

namespace PathTracer
{
    struct VulkanDevice
    {
        VkPhysicalDevice            physical_device_ = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties  physical_device_props_ = {};
        VkDevice                    device_ = VK_NULL_HANDLE;
        operator VkDevice()
        {
            return device_;
        }
    };

    struct Application::VulkanManager
    {
        VulkanManager() = default;
        ~VulkanManager() = default;

        bool Init(Window& window);
        void Terminate();
        VkSemaphore& SignalSemaphore();
        VkSemaphore& WaitSemaphore();

        VulkanDevice                device_;
        VkInstance                  instance_ = VK_NULL_HANDLE;
        VkDescriptorPool            descriptor_pool_ = VK_NULL_HANDLE;
        VkPipelineCache             pipeline_cache_ = VK_NULL_HANDLE;
        VkQueue                     queue_ = VK_NULL_HANDLE;
        VkCommandPool               command_pool_ = VK_NULL_HANDLE;
        std::vector<VkSemaphore>    semaphores_;
        uint32_t                    semaphore_index_ = 0;
        // The swap chain info
        std::vector<VkImage>        swap_chain_images_;
        uint32_t                    swap_chain_image_index_ = 0;
        VkFormat                    swap_chain_image_format_ = VK_FORMAT_UNDEFINED;
        VkExtent2D                  extent_ = {};
        VkSurfaceKHR                surface_ = VK_NULL_HANDLE;
        VkSwapchainKHR              swap_chain_ = VK_NULL_HANDLE;
        VkSurfaceFormatKHR          surface_format_ = {};
    };

}
