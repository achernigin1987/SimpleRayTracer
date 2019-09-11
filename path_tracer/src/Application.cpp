#include "Application.h"
#include "VulkanManager.h"
#include "AccelerationStructureController.h"

namespace PathTracer
{

    Application::Application()
        : vulkan_manager_(std::make_unique<VulkanManager>())
    {}

    Application::~Application()
    {}

    // Gets the semaphore to be waiting on
    VkSemaphore& Application::WaitSemaphore()
    {
        return vulkan_manager_->WaitSemaphore();
    }

    // Gets the semaphore to be signalling to
    VkSemaphore& Application::SignalSemaphore()
    {
        return vulkan_manager_->SignalSemaphore();
    }

    int32_t Application::Run(int32_t argc, char const** argv)
    {
        window_ = std::make_unique<Window>();
        // Initialize Vulkan
        if (!vulkan_manager_->Init(*window_))
        {
            return EXIT_FAILURE;
        }

        Init(argc, argv);

        // Application loop
        while (!glfwWindowShouldClose(*window_))
        {
            // Acquire the next image from the swap chain
            vkAcquireNextImageKHR(vulkan_manager_->device_, vulkan_manager_->swap_chain_, std::numeric_limits<uint64_t>::max(),
                                  SignalSemaphore(), VK_NULL_HANDLE, &vulkan_manager_->swap_chain_image_index_);

            // Do frame
            glfwPollEvents();
            Update();
            Render();

            // Present the final image
            VkPresentInfoKHR present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &WaitSemaphore();
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &vulkan_manager_->swap_chain_;
            present_info.pImageIndices = &vulkan_manager_->swap_chain_image_index_;
            if (vkQueuePresentKHR(vulkan_manager_->queue_, &present_info) != VK_SUCCESS)
            {
                std::cerr << "Fatal error: Unable to present" << std::endl;
                break;
            }
            //Pipeline::AdvancePool();
        }

        // Wait until the device has completed all the work
        vkQueueWaitIdle(vulkan_manager_->queue_);

        // Terminate the application
        Terminate();
        vulkan_manager_->Terminate();

        // Tear down the window
        window_ = nullptr;

        return EXIT_SUCCESS;
    }
    VkResult Application::Init(int32_t argc, char const ** argv)
    {


        return VK_SUCCESS;
    }
}