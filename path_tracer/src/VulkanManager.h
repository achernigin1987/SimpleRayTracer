#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <map>

#include "Window.h"

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
        CommandBuffer(VkCommandPool command_pool, VkDevice device);
        void Begin() const;
        void End() const;
        operator VkCommandBuffer() const
        {
            return command_buffer_.get();
        }
        VkResult Submit(VkQueue queue, VkSemaphore* wait = nullptr, uint32_t wait_count = 0u,
                        VkSemaphore* signal = nullptr, uint32_t signal_count = 0u,
                        VkFence fence = VK_NULL_HANDLE);
        VkResult SubmitWait(VkQueue queue, VkSemaphore* wait = nullptr, uint32_t wait_count = 0u,
                            VkSemaphore* signal = nullptr, uint32_t signal_count = 0u,
                            VkFence fence = VK_NULL_HANDLE);
        inline VkCommandBuffer Get() const
        {
            return command_buffer_.get();
        }

    private:
        VkScopedObject<VkCommandBuffer> command_buffer_;
    };

    struct VulkanManager
    {
        VulkanManager() = default;
        ~VulkanManager();
        // Non-copyable
        VulkanManager(VulkanManager const&) = delete;
        VulkanManager& operator =(VulkanManager const&) = delete;
        VulkanManager(VulkanManager&&) = delete;
        VulkanManager& operator=(VulkanManager&&) = delete;

        bool Init(Window& window);
        VkSemaphore& SignalSemaphore();
        VkSemaphore& WaitSemaphore();
        std::uint32_t FindDeviceMemoryIndex(VkMemoryPropertyFlags flags) const;
        VkScopedObject<VkDeviceMemory> AllocateDeviceMemory(std::uint32_t memory_type_index, std::size_t size) const;
        VkScopedObject<VkBuffer> CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage) const;
        std::vector<CommandBuffer> CreateBlitCommandBuffers(VkBuffer buffer, Window const& window) const;
        VkMemoryRequirements GetBufferMemoryRequirements(VkBuffer buffer) const;

        void BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) const;
        void* MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size) const;
        void UnmapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size) const;
        void EncodeCopyBuffer(VkBuffer src_buffer,
                              VkBuffer dst_buffer,
                              VkDeviceSize src_offset,
                              VkDeviceSize dst_offset,
                              VkDeviceSize size,
                              VkCommandBuffer& command_buffer) const;
        void EncodeBufferBarrier(VkBuffer buffer,
                                 VkAccessFlags src_access,
                                 VkAccessFlags dst_access,
                                 VkPipelineStageFlags src_stage,
                                 VkPipelineStageFlags dst_stage,
                                 VkCommandBuffer& command_buffer) const;
        void EncodeBufferBarriers(VkBuffer const* buffers,
                                  std::uint32_t buffer_count,
                                  VkAccessFlags src_access,
                                  VkAccessFlags dst_access,
                                  VkPipelineStageFlags src_stage,
                                  VkPipelineStageFlags dst_stage,
                                  VkCommandBuffer& command_buffer) const;
        void CreateImage(uint32_t width,
                         uint32_t height,
                         VkFormat format,
                         VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         VkScopedObject <VkImage>& image,
                         VkScopedObject<VkDeviceMemory>& imageMemory) const;

        void CreateTextureImage(uint8_t const* pixels,
                                int texWidth,
                                int texHeight,
                                int texChannels,
                                VkScopedObject <VkImage>& textureImage,
                                VkScopedObject<VkDeviceMemory>& textureImageMemory) const;

        void TransitionImageLayout(VkImage image,
                                   VkFormat format,
                                   VkImageLayout oldLayout,
                                   VkImageLayout newLayout) const;

        void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
        VkScopedObject<VkImageView> CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
        VkScopedObject<VkSampler> CreateTextureSampler() const;

        VkScopedObject<VkFence> CreateFence() const;

        template <typename T>
        std::pair<VkScopedObject<VkBuffer>, VkScopedObject<VkDeviceMemory>> CreateAllocatedBuffer(VkBufferUsageFlags usage,
                                                                                                  VkMemoryPropertyFlags flags,
                                                                                                  std::vector<T> const& data) const
        {
            VkScopedObject<VkBuffer> buffer = CreateBuffer(data.size() * sizeof(T), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            uint32_t mem_type_index = FindDeviceMemoryIndex(flags);

            VkMemoryRequirements mem_reqs = GetBufferMemoryRequirements(buffer.get());

            VkScopedObject<VkDeviceMemory> memory = AllocateDeviceMemory(mem_type_index, mem_reqs.size);

            vkBindBufferMemory(device_, buffer.get(), memory.get(), 0);

            if (!data.empty())
            {
                if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                {
                    void* dest = MapMemory(memory.get(), 0u, mem_reqs.size);
                    memcpy(dest, data.data(), sizeof(T) * data.size());
                    UnmapMemory(memory.get(), 0u, sizeof(T) * data.size());
                }
                else
                {
                    auto staging_pair = CreateAllocatedBuffer<T>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, data);

                    CommandBuffer command_buffer(command_pool_, device_);
                    VkCommandBuffer cmd = command_buffer.Get();

                    command_buffer.Begin();
                    EncodeCopyBuffer(staging_pair.first.get(), buffer.get(), 0u, 0u, mem_reqs.size, cmd);
                    EncodeBufferBarrier(buffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cmd);
                    command_buffer.End();
                    command_buffer.SubmitWait(queue_);
                }
            }

            return std::make_pair(buffer, memory);
        }

        template<typename T>
        static T align(T value, T alignment)
        {
            return (value + (alignment - 1)) / alignment * alignment;
        }

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
    private:
        void Terminate();
    };

}
