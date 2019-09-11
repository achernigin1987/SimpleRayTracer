#pragma once
#include "Application.h"

namespace PathTracer
{
    template <typename T> using VkScopedObject = std::shared_ptr<std::remove_pointer_t<T>>;

    struct VulkanDevice
    {
        VkPhysicalDevice            physical_device_ = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties  physical_device_props_ = {};
        VkDevice                    device_ = VK_NULL_HANDLE;
        operator VkDevice() const
        {
            return device_;
        }
    };

    class CommandBuffer
    {
    public:
        CommandBuffer(VkCommandPool command_pool, VkDevice device)
        {
            VkCommandBuffer raw_command_buffer = nullptr;
            VkCommandBufferAllocateInfo command_buffer_info;
            command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_buffer_info.pNext = nullptr;
            command_buffer_info.commandPool = command_pool;
            command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_buffer_info.commandBufferCount = 1u;

            auto status = vkAllocateCommandBuffers(device,
                                                   &command_buffer_info,
                                                   &raw_command_buffer);
            if (status != VK_SUCCESS)
            {
                throw std::runtime_error("Cannot allocate command buffer");
            }

            command_buffer_ = VkScopedObject<VkCommandBuffer>(raw_command_buffer,
                                                             [&](VkCommandBuffer command_buffer)
            {
                vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
            });
        }
        void Begin() const
        {
            VkCommandBufferBeginInfo begin_info;
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.pNext = nullptr;
            begin_info.flags = 0;
            begin_info.pInheritanceInfo = nullptr;

            vkBeginCommandBuffer(command_buffer_.get(), &begin_info);
        }
        void End() const
        {
            vkEndCommandBuffer(command_buffer_.get());
        }
        operator VkCommandBuffer() const
        {
            return command_buffer_.get();
        }
        VkResult Submit(VkQueue queue, VkSemaphore* wait = nullptr, uint32_t wait_count = 0u,
                        VkSemaphore* signal = nullptr, uint32_t signal_count = 0u,
                        VkFence fence = VK_NULL_HANDLE)
        {
            VkCommandBuffer cmd_buf = command_buffer_.get();
            VkSubmitInfo submit_info;
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            submit_info.waitSemaphoreCount = wait_count;
            submit_info.pWaitSemaphores = wait;
            submit_info.pWaitDstStageMask = nullptr;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &cmd_buf;
            submit_info.signalSemaphoreCount = signal_count;
            submit_info.pSignalSemaphores = signal;

            return vkQueueSubmit(queue, 1, &submit_info, fence);
        }
        VkResult SubmitWait(VkQueue queue, VkSemaphore* wait = nullptr, uint32_t wait_count = 0u,
                            VkSemaphore* signal = nullptr, uint32_t signal_count = 0u,
                            VkFence fence = VK_NULL_HANDLE)
        {
            auto res = Submit(queue, wait, wait_count, signal, signal_count, fence);
            if (res != VK_SUCCESS)
            {
                return res;
            }
            return vkQueueWaitIdle(queue);
        }

        // Non-copyable
        CommandBuffer(CommandBuffer const&) = delete;
        CommandBuffer& operator =(CommandBuffer const&) = delete;
        CommandBuffer(CommandBuffer&&) = delete;
        CommandBuffer& operator=(CommandBuffer&&) = delete;

    private:
        VkScopedObject<VkCommandBuffer> command_buffer_;
    };

    struct Application::VulkanManager
    {
        VulkanManager() = default;
        ~VulkanManager() = default;
        // Non-copyable
        VulkanManager(VulkanManager const&) = delete;
        VulkanManager& operator =(VulkanManager const&) = delete;
        VulkanManager(VulkanManager&&) = delete;
        VulkanManager& operator=(VulkanManager&&) = delete;

        bool Init(Window& window);
        void Terminate();
        VkSemaphore& SignalSemaphore();
        VkSemaphore& WaitSemaphore();
        std::uint32_t FindDeviceMemoryIndex(VkMemoryPropertyFlags flags) const;
        VkDeviceMemory AllocateDeviceMemory(std::uint32_t memory_type_index, std::size_t size) const;


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
