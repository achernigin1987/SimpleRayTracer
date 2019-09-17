#include "VulkanManager.h"
#include <iostream>
#include <algorithm>

namespace
{
    // Vulkan setup
    std::vector<const char *> validation_layers =
    {
        "VK_LAYER_LUNARG_standard_validation"
    };

    std::vector<const char *> required_extensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Reports the Vulkan error messages
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugReportFlagsEXT,
        VkDebugReportObjectTypeEXT,
        uint64_t,
        size_t,
        int32_t,
        char const*,
        char const* msg,
        void*)
    {
        std::cerr << "Vulkan: " << msg << std::endl;
        return VK_FALSE;
    }

    // Creates the error reporting callback
    VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
    {
        auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        if (func == nullptr)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }

    // Destroys the error reporting callback
    void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator)
    {
        auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if (func != nullptr)
        {
            func(instance, callback, pAllocator);
        }
    }

    bool CreateInstance(GLFWwindow* window, VkInstance& instance, bool has_validation_layer)
    {
        // Create the instance
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Path Tracer";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "RadeonRaysNext";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        // Query the extensions required by glfw
        uint32_t glfw_extension_count = 0;

        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char *> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        // Enable validation layers if requested
        std::vector<const char *> compatible_layers;

        if (!validation_layers.empty())
        {
            // Retrieve the available validation layers
            uint32_t layer_count = 0u;
            vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
            std::vector<VkLayerProperties> available_layers(layer_count);
            vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

            // Select the subset of compatible layers
            for (auto validation_layer : validation_layers)
            {
                for (auto available_layer : available_layers)
                    if (!strcmp(validation_layer, available_layer.layerName))
                    {
                        compatible_layers.push_back(validation_layer);
                        if (!strcmp(validation_layer, "VK_LAYER_LUNARG_standard_validation"))
                        {
                            has_validation_layer = true;
                        }
                        break;
                    }
            }

            // Enable the validation layers
            create_info.enabledLayerCount = static_cast<uint32_t>(compatible_layers.size());
            create_info.ppEnabledLayerNames = compatible_layers.data();
            if (has_validation_layer)
            {
                required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            }
        }

        // Try enabling the required extensions
        create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

        // Create the Vulkan instance
        if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS)
        {
            std::cerr << "Fatal error: Could not create Vulkan instance" << std::endl;
            return false;
        }
        return true;
    }

    bool InitDevice(PathTracer::VulkanDevice& device, VkQueue& queue,
                    VkSurfaceCapabilitiesKHR& capabilities,
                    std::vector<VkSurfaceFormatKHR>& formats,
                    std::vector<VkPresentModeKHR>& present_modes,
                    uint32_t& queue_family_index,
                    VkInstance instance, VkSurfaceKHR surface)
    {
        // Enumerate the available physical devices
        uint32_t physical_device_count = 0u;
        vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
        if (!physical_device_count)
        {
            std::cerr << "Fatal error: No physical device with Vulkan support is available" << std::endl;
            return false;
        }
        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

        // Find a suitable Vulkan device
        auto physical_device_index = 0u;
        bool device_ok = false;

        for (; physical_device_index < physical_devices.size(); ++physical_device_index)
        {
            VkPhysicalDevice physical_device = physical_devices[physical_device_index];

            // Retrieve the number of queue families available on this device
            uint32_t queue_family_count = 0u;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

            // Iterate the available queue families
            queue_family_index = 0u;
            for (auto &queue_family : queue_families)
            {
                if (!queue_family.queueCount)
                {
                    continue;   // no queue available
                }
                // Check for presenting support
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface, &presentSupport);
                if (presentSupport && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    && (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)
                    && (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT))
                {
                    device_ok = true;
                    break;
                }

                // Iterate over to the next queue family
                ++queue_family_index;
            }

            // At a minimum we need a graphics queue and a present queue
            if (!device_ok)
            {
                continue;   // physical device is not suitable
            }

            // Check that the device support the required extensions
            uint32_t extension_count = 0u;
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> available_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

            bool support_required_extensions = true;
            for (auto required_extension : required_extensions)
            {
                bool support_required_extension = false;
                for (auto available_extension : available_extensions)
                {
                    if (!strcmp(required_extension, available_extension.extensionName))
                    {
                        support_required_extension = true;
                        break;
                    }
                }
                if (!support_required_extension)
                {
                    support_required_extensions = false;
                    break;
                }
            }
            if (!support_required_extensions)
            {
                continue;   // physical device is not suitable
            }

            // Query swap chain support
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

            uint32_t format_count = 0u;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
            formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

            uint32_t present_mode_count = 0u;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
            present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());

            if (formats.empty() || present_modes.empty())
            {
                continue;   // physical device is not suitable
            }

            break;  // done
        }

        if (physical_device_index == physical_devices.size())
        {
            std::cerr << "Fatal error: Unable to find a suitable Vulkan device" << std::endl;
            return false;
        }
        device.physical_device_ = physical_devices[physical_device_index];

        // Create a logical device
        float queue_priority = 0.f;
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index;
        queue_create_info.queueCount = 1u;
        queue_create_info.pQueuePriorities = &queue_priority;

        vkGetPhysicalDeviceProperties(device.physical_device_, &device.physical_device_props_);

        // Create the Vulkan device
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.shaderInt64 = VK_TRUE;

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1u;
        createInfo.pQueueCreateInfos = &queue_create_info;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        createInfo.ppEnabledExtensionNames = required_extensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        createInfo.ppEnabledLayerNames = validation_layers.data();

        if (vkCreateDevice(device.physical_device_, &createInfo, nullptr, &device.device_) != VK_SUCCESS)
        {
            std::cerr << "Fatal error: Unable to create Vulkan device" << std::endl;
            return false;
        }

        // Retrieve the work queues
        vkGetDeviceQueue(device, queue_family_index, 0, &queue);

        return true;
    }

    // The error reporting callback
    VkDebugReportCallbackEXT callback;
}

namespace PathTracer
{
    VulkanManager::~VulkanManager()
    {
        Terminate();
    }
    bool VulkanManager::Init(Window& window)
    {
        if (!window) return false;

        bool has_validation_layer = false;
        if (!CreateInstance(window, instance_, has_validation_layer)) return false;

        // Create the error reporting callback
        if (has_validation_layer)
        {
            VkDebugReportCallbackCreateInfoEXT createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
            createInfo.pfnCallback = DebugCallback;

            if (CreateDebugReportCallbackEXT(instance_, &createInfo, nullptr, &callback) != VK_SUCCESS)
            {
                std::cerr << "Fatal error: Unable to create the error reporting callback" << std::endl;
                return false;
            }
        }

        // Create the window surface
        if (glfwCreateWindowSurface(instance_, window, nullptr, &surface_) != VK_SUCCESS)
        {
            std::cerr << "Fatal error: Unable to create the Vulkan window surface" << std::endl;
            return false;
        }

        // Information about the selected physical device
        VkSurfaceCapabilitiesKHR capabilities = {};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
        uint32_t queue_family_index = 0xFFFFFFFF;

        if (!InitDevice(device_, queue_,
                        capabilities,
                        formats,
                        present_modes,
                        queue_family_index,
                        instance_, surface_))
        {
            std::cerr << "Fatal error: Unable to create the Vulkan device" << std::endl;
            return false;
        }

        // Create the swap chain
        {
            VkSurfaceFormatKHR format = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_MAX_ENUM_KHR };
            if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
            {
                format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
            }
            else
            {
                for (auto &availableFormat : formats)
                {
                    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    {
                        format = availableFormat;
                        break;  // found preferred surface format
                    }
                }
                if (format.format == VK_FORMAT_UNDEFINED)
                {
                    format = formats[0];    // fallback to first format in list
                }
            }

            // Select the presenting mode
            VkPresentModeKHR present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
            for (auto availablePresentMode : present_modes)
            {
                if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR)
                {
                    present_mode = VK_PRESENT_MODE_FIFO_KHR;
                }
            }
            if (present_mode == VK_PRESENT_MODE_MAX_ENUM_KHR)
            {
                present_mode = present_modes[0];
            }

            // Set the surface extent
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                extent_ = capabilities.currentExtent;
            }
            else
            {
                extent_ = { window.window_width_, window.window_height_ };
                extent_.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent_.width));
                extent_.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent_.height));
            }

            // Set the swap chain properties
            auto image_count = capabilities.minImageCount + 1;
            if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
            {
                image_count = capabilities.maxImageCount;
            }
            surface_format_ = format;

            // Create the swap chain
            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface_;
            createInfo.minImageCount = image_count;
            createInfo.imageFormat = surface_format_.format;
            createInfo.imageColorSpace = surface_format_.colorSpace;
            createInfo.imageExtent = extent_;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.preTransform = capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = present_mode;
            createInfo.clipped = VK_TRUE;

            if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swap_chain_) != VK_SUCCESS)
            {
                std::cerr << "Fatal error: Unable to create swap chain" << std::endl;
                return false;
            }

            // Retrieve the swap chain images
            vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, nullptr);
            swap_chain_images_.resize(image_count);
            vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, swap_chain_images_.data());

            swap_chain_image_format_ = surface_format_.format;
        }

        // Create the descriptor pool
        {
            const VkDescriptorPoolSize poolSizes[] =
            {
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            32 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    64 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            32 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             32 }
            };

            VkDescriptorPoolCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            createInfo.maxSets = 32;
            createInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(*poolSizes));
            createInfo.pPoolSizes = poolSizes;

            if (vkCreateDescriptorPool(device_, &createInfo, nullptr, &descriptor_pool_))
            {
                std::cerr << "Fatal error: Unable to create the descriptor pool" << std::endl;
                return false;
            }
        }

        // Create the pipeline cache
        {
            VkPipelineCacheCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
            if (vkCreatePipelineCache(device_, &createInfo, nullptr, &pipeline_cache_) != VK_SUCCESS)
            {
                std::cerr << "Fatal error: Unable to create the pipeline cache" << std::endl;
                return false;
            }
        }

        // Create the command pools
        {
            VkCommandPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            pool_info.queueFamilyIndex = queue_family_index;
            if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
            {
                std::cerr << "Fatal error: Unable to create the command pool" << std::endl;
                return false;
            }
        }

        // Create the synchronization primitives
        {
            semaphores_.resize(2);
            VkSemaphoreCreateInfo semaphore_info = {};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            for (auto &semaphore : semaphores_)
            {
                if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS)
                {
                    std::cerr << "Fatal error: Unable to create the synchronization primitives" << std::endl;
                    return false;
                }
            }
        }

        // Set the initial image layouts
        {
            CommandBuffer command_buffer(command_pool_, device_);
            command_buffer.Begin();

            uint32_t i = 0;
            std::vector<VkImageMemoryBarrier> image_memory_barriers(swap_chain_images_.size());
            for (auto& image_memory_barrier : image_memory_barriers)
            {
                image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                image_memory_barrier.image = swap_chain_images_[i++];
                image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_memory_barrier.subresourceRange.levelCount = 1;
                image_memory_barrier.subresourceRange.layerCount = 1;
            }
            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<uint32_t>(image_memory_barriers.size()),
                image_memory_barriers.data());

            command_buffer.End();

            command_buffer.SubmitWait(queue_);
        }

        return true;
    }

    VkScopedObject<VkFence> VulkanManager::CreateFence() const
    {
        VkFence fence;
        // Create the fence
        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkResult result = vkCreateFence(device_, &fence_create_info, nullptr, &fence);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create fence");
        }

        return VkScopedObject<VkFence>(fence, [this](VkFence fence)
        {
            vkDestroyFence(device_, fence, nullptr);
        });
    }

    void VulkanManager::Terminate()
    {
        if (callback)
        {
            DestroyDebugReportCallbackEXT(instance_, callback, nullptr);
        }
        for (auto semaphore : semaphores_)
        {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
        vkDestroyCommandPool(device_, command_pool_, nullptr);

        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);
        vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyDevice(device_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    VkSemaphore& VulkanManager::SignalSemaphore()
    {
        return semaphores_[semaphore_index_];
    }

    VkSemaphore& VulkanManager::WaitSemaphore()
    {
        VkSemaphore& semaphore = semaphores_[semaphore_index_];
        semaphore_index_ = (semaphore_index_ + 1) % static_cast<uint32_t>(semaphores_.size());
        return semaphore;
    }

    std::uint32_t VulkanManager::FindDeviceMemoryIndex(VkMemoryPropertyFlags flags) const
    {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(device_.physical_device_, &mem_props);

        for (auto i = 0u; i < mem_props.memoryTypeCount; i++)
        {
            auto& memory_type = mem_props.memoryTypes[i];
            if ((memory_type.propertyFlags & flags) == flags)
            {
                return i;
            }
        }

        throw std::runtime_error("Cannot find specified memory type");
    }

    VkScopedObject<VkDeviceMemory> VulkanManager::AllocateDeviceMemory(std::uint32_t memory_type_index, std::size_t size) const
    {
        VkMemoryAllocateInfo info;
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = nullptr;
        info.memoryTypeIndex = memory_type_index;
        info.allocationSize = size;

        VkDeviceMemory memory = nullptr;
        auto res = vkAllocateMemory(device_.device_, &info, nullptr, &memory);

        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot allocate device memory");
        }

        return VkScopedObject<VkDeviceMemory>(memory,
                                              [device = device_](VkDeviceMemory memory)
        {
            vkFreeMemory(device, memory, nullptr);
        });
    }

    VkScopedObject<VkBuffer> VulkanManager::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage) const
    {
        VkBufferCreateInfo buffer_create_info;
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.pNext = nullptr;
        buffer_create_info.usage = usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_create_info.size = size;
        buffer_create_info.flags = 0;
        buffer_create_info.queueFamilyIndexCount = 0u;
        buffer_create_info.pQueueFamilyIndices = nullptr;

        VkBuffer buffer = nullptr;
        auto res = vkCreateBuffer(device_, &buffer_create_info, nullptr, &buffer);

        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create Vulkan buffer");
        }

        return VkScopedObject<VkBuffer>(buffer, [this](VkBuffer buffer)
        {
            vkDestroyBuffer(device_, buffer, nullptr);
        });
    }

    VkMemoryRequirements VulkanManager::GetBufferMemoryRequirements(VkBuffer buffer) const
    {
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device_, buffer, &mem_reqs);
        return mem_reqs;
    }

    void VulkanManager::BindBufferMemory(VkBuffer buffer,
                                         VkDeviceMemory memory,
                                         VkDeviceSize offset) const
    {
        auto res = vkBindBufferMemory(device_,
                                      buffer,
                                      memory,
                                      offset);

        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot bind buffer memory");
        }
    }


    void* VulkanManager::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size) const
    {
        void* mapped_ptr = nullptr;

        auto res = vkMapMemory(device_, memory, offset, size, 0, &mapped_ptr);

        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("GPUServices: Cannot map host visible buffer");
        }

        return mapped_ptr;
    }

    void VulkanManager::UnmapMemory(VkDeviceMemory memory, VkDeviceSize, VkDeviceSize) const
    {
        vkUnmapMemory(device_, memory);
    }

    void VulkanManager::EncodeCopyBuffer(VkBuffer src_buffer,
                                         VkBuffer dst_buffer,
                                         VkDeviceSize src_offset,
                                         VkDeviceSize dst_offset,
                                         VkDeviceSize size,
                                         VkCommandBuffer& command_buffer) const
    {
        VkBufferCopy copy_region;
        copy_region.srcOffset = src_offset;
        copy_region.dstOffset = dst_offset;
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1u, &copy_region);
    }

    void VulkanManager::EncodeBufferBarrier(VkBuffer buffer,
                                            VkAccessFlags src_access,
                                            VkAccessFlags dst_access,
                                            VkPipelineStageFlags src_stage,
                                            VkPipelineStageFlags dst_stage,
                                            VkCommandBuffer& command_buffer) const
    {
        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.buffer = buffer;
        barrier.offset = 0u;
        barrier.size = VK_WHOLE_SIZE;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = src_access;
        barrier.dstAccessMask = dst_access;

        vkCmdPipelineBarrier(command_buffer,
                             src_stage,
                             dst_stage,
                             0u,
                             0u,
                             nullptr,
                             1u,
                             &barrier,
                             0u,
                             nullptr);
    }

    void VulkanManager::EncodeBufferBarriers(VkBuffer const* buffers,
                                             std::uint32_t buffer_count,
                                             VkAccessFlags src_access,
                                             VkAccessFlags dst_access,
                                             VkPipelineStageFlags src_stage,
                                             VkPipelineStageFlags dst_stage,
                                             VkCommandBuffer& command_buffer) const
    {
        VkBufferMemoryBarrier* barriers = static_cast<VkBufferMemoryBarrier*>(
            alloca(buffer_count * sizeof(VkBufferMemoryBarrier)));

        for (auto i = 0u; i < buffer_count; ++i)
        {
            auto& barrier = barriers[i];
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.buffer = buffers[i];
            barrier.offset = 0u;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcAccessMask = src_access;
            barrier.dstAccessMask = dst_access;
        }

        vkCmdPipelineBarrier(command_buffer,
                             src_stage,
                             dst_stage,
                             0u,
                             0u,
                             nullptr,
                             buffer_count,
                             barriers,
                             0u,
                             nullptr);
    }

    // Encodes the commands for blitting to the swap chain images
    std::vector<CommandBuffer> VulkanManager::CreateBlitCommandBuffers(VkBuffer buffer, Window const& window) const
    {
        std::vector<CommandBuffer> blit_command_buffers;
        uint32_t swap_chain_image_index = 0;
        for (size_t i = 0; i < swap_chain_images_.size(); i++)
        {
            blit_command_buffers.push_back(CommandBuffer(command_pool_, device_));
            blit_command_buffers.back().Begin();

            // Transition image for blit
            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.image = swap_chain_images_[swap_chain_image_index];
            imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.levelCount = 1;
            imageMemoryBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                blit_command_buffers.back().Get(),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageMemoryBarrier);

            // Copy results to swap chain image
            VkBufferImageCopy buffer_image_copy = {};
            buffer_image_copy.bufferRowLength = window.window_width_ * sizeof(uint32_t) >> 2;
            buffer_image_copy.bufferImageHeight = window.window_height_;
            buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            buffer_image_copy.imageSubresource.layerCount = 1;
            buffer_image_copy.imageExtent.width = window.window_width_;
            buffer_image_copy.imageExtent.height = window.window_height_;
            buffer_image_copy.imageExtent.depth = 1;
            vkCmdCopyBufferToImage(
                blit_command_buffers.back().Get(),
                buffer,
                swap_chain_images_[swap_chain_image_index],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &buffer_image_copy);

            // Transition to present layout
            imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageMemoryBarrier.image = swap_chain_images_[swap_chain_image_index++];
            imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.levelCount = 1;
            imageMemoryBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                blit_command_buffers.back().Get(),
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageMemoryBarrier);

            blit_command_buffers.back().End();
        }

        return blit_command_buffers;
    }

    CommandBuffer::CommandBuffer(VkCommandPool command_pool, VkDevice device)
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
            if (command_buffer)
            {
                //vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
            }
        });
    }

    void CommandBuffer::Begin() const
    {
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(command_buffer_.get(), &begin_info);
    }

    void CommandBuffer::End() const
    {
        vkEndCommandBuffer(command_buffer_.get());
    }

    VkResult CommandBuffer::Submit(VkQueue queue, VkSemaphore * wait, uint32_t wait_count, VkSemaphore * signal, uint32_t signal_count, VkFence fence)
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

    VkResult CommandBuffer::SubmitWait(VkQueue queue, VkSemaphore * wait, uint32_t wait_count, VkSemaphore * signal, uint32_t signal_count, VkFence fence)
    {
        auto res = Submit(queue, wait, wait_count, signal, signal_count, fence);
        if (res != VK_SUCCESS)
        {
            return res;
        }
        return vkQueueWaitIdle(queue);
    }
}