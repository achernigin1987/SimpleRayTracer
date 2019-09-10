#include "VulkanManager.h"
#include <iostream>
#include <algorithm>

namespace
{
    // Vulkan setup
#ifndef _DEBUG
    std::vector<const char *> validation_layers;
#else // _DEBUG
    std::vector<const char *> validation_layers =
    {
        "VK_LAYER_LUNARG_standard_validation"
    };
#endif // _DEBUG
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
    bool Application::VulkanManager::Init(Window& window)
    {
        if (!window) return false;

        bool has_validation_layer = false;
        if (CreateInstance(window, instance_, has_validation_layer)) return false;

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

        if (InitDevice(device_, queue_,
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
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;

            VkCommandBufferAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.commandPool = command_pool_;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandBufferCount = 1;
            vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(command_buffer, &begin_info);
            {
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
            }
            vkEndCommandBuffer(command_buffer);

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &command_buffer;
            vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(queue_);

            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        }

        return true;
    }

    void Application::VulkanManager::Terminate()
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

    VkSemaphore& Application::VulkanManager::SignalSemaphore()
    {
        return semaphores_[semaphore_index_];
    }
    VkSemaphore& Application::VulkanManager::WaitSemaphore()
    {
        VkSemaphore& semaphore = semaphores_[semaphore_index_];
        semaphore_index_ = (semaphore_index_ + 1) % static_cast<uint32_t>(semaphores_.size());
        return semaphore;
    }

}