#define _USE_MATH_DEFINES

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#include "Application.h"
#include "AccelerationStructureController.h"
#include "SceneController.h"

namespace PathTracer
{

    Application::Application()
        : vulkan_manager_(std::make_unique<VulkanManager>())
        , orbit_(glm::vec3(500.0f, 400.0f, 0.0f), glm::vec3(-200.0f, 300.0f, 0.0f))
        , view_projection_(0.0f)
        , sample_count_(0)
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
        }

        // Wait until the device has completed all the work
        vkQueueWaitIdle(vulkan_manager_->queue_);

        // Terminate the application
        vulkan_manager_->Terminate();
        as_controller_ = nullptr;

        // Tear down the window
        window_ = nullptr;

        return EXIT_SUCCESS;
    }
    VkResult Application::Init(int32_t argc, char const ** argv)
    {
        std::string filename;
        for (int32_t i = 1; i < argc; i++)
        {
            auto parameter = std::string(argv[i]);
            if (parameter == "--scene" || parameter == "-s")
            {
                if (i + 1 < argc)
                {
                    filename = argv[i + 1];
                    break;
                }
            }
        }
        if (filename.empty())
        {
            throw std::runtime_error("Path to scene not set");
        }

        as_controller_ = std::make_unique<AccelerationStructureController>(vulkan_manager_);

        Scene scene;
        scene.LoadFile(filename.c_str());
        as_controller_->BuildAccelerationStructure(scene);
        path_tracer_ = std::make_unique<PathTracerImpl>(vulkan_manager_);
        path_tracer_->Init(scene, as_controller_->Get(), as_controller_->GetContext(), window_->window_width_ * window_->window_height_);
        blit_cmd_buffers_ = vulkan_manager_->CreateBlitCommandBuffers(path_tracer_->GetColor(), *window_);

        InitCallbacks();

        return VK_SUCCESS;
    }

    VkResult Application::Update()
    {
        // Compute view/projection matrices
        auto near_far = glm::vec2(0.1f, 10000.0f);
        auto view = glm::lookAt(orbit_.Eye(), orbit_.Center(), orbit_.Up());
        auto screen_dims = glm::vec4(static_cast<float>(window_->window_width_),
                                     static_cast<float>(window_->window_height_),
                                     1.0f / static_cast<float>(window_->window_width_),
                                     1.0f / static_cast<float>(window_->window_height_));
        auto proj = glm::perspective(60.0f * static_cast<float>(M_PI) / 180.0f,
                                     screen_dims.x / screen_dims.y, near_far.x, near_far.y);
        auto view_projection = proj * view;
        auto view_proj_inv = glm::inverse(view_projection);

        // Reset accumulation if camera has moved
        if (view_projection != view_projection_)
        {
            sample_count_ = 0;
        }
        view_projection_ = view_projection;

        Params params;
        memcpy(params.eye_, &orbit_.Eye()[0], 3 * sizeof(float));
        params.eye_[3] = static_cast<float>(sample_count_++);
        memcpy(params.center_, &orbit_.Center()[0], 3 * sizeof(float));
        memcpy(params.near_far_, &near_far[0], 2 * sizeof(float));
        memcpy(params.screen_dims_, &screen_dims[0], 4 * sizeof(float));
        memcpy(params.view_proj_inv_, &view_proj_inv[0][0], 16 * sizeof(float));

        path_tracer_->UpdateView(params);

        return VK_SUCCESS;
    }

    // Renders the ambient occlusion example
    VkResult Application::Render()
    {
        // Submit the command buffers
        path_tracer_->Submit();
        SubmitBlitCommandBuffer(blit_cmd_buffers_, vulkan_manager_->queue_);

        return VK_SUCCESS;
    }

    // Submits the blit command buffer
    VkResult Application::SubmitBlitCommandBuffer(std::vector<CommandBuffer> const& blit_command_buffers, VkQueue queue, VkFence fence)
    {
        VkPipelineStageFlags stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkCommandBuffer cmd_buffer = blit_command_buffers[vulkan_manager_->swap_chain_image_index_].Get();

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &WaitSemaphore();
        submit_info.pWaitDstStageMask = &stage_flags;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &SignalSemaphore();

        return vkQueueSubmit(queue, 1, &submit_info, fence);
    }

    // A helper for keeping track of the mouse state
    class MouseState
    {
    public:
        // Non-copyable
        MouseState(MouseState const&) = delete;
        MouseState& operator =(MouseState const&) = delete;
        MouseState(MouseState&&) = delete;
        MouseState& operator=(MouseState&&) = delete;
        static MouseState& Get()
        {
            static MouseState state;
            return state;
        }
    private:
        // Constructor
        MouseState()
            : pressed_buttons_(0)
        {
        }
    public:
        // A bitmask of the currently pressed mouse buttons
        uint32_t pressed_buttons_;
        // The previous mouse on-screen position
        glm::vec2 previous_position_;
    };

    // Binds the callbacks for listening to the user inputs
    VkResult Application::InitCallbacks()
    {

        glfwSetMouseButtonCallback(*window_,
                                   [](GLFWwindow *, int32_t button, int32_t action, int32_t modifiers)
        {
            GetAppInstance().OnMousePress(button, action, modifiers);
        });

        glfwSetCursorPosCallback(*window_,
                                 [](GLFWwindow *, double x_pos, double y_pos)
        {
            GetAppInstance().OnMouseMove(glm::vec2(static_cast<float>(x_pos), static_cast<float>(y_pos)));
        });

        glfwSetScrollCallback(*window_,
                              [](GLFWwindow *, double, double y_offset)
        {
            GetAppInstance().OnMouseScroll(static_cast<float>(y_offset));
        });

        return VK_SUCCESS;
    }


    // Callback function for scrolling events
    void Application::OnMouseScroll(float scroll)
    {
        auto distance_to_pivot_point = glm::length(orbit_.Eye() - orbit_.Center());
        auto distance = scroll * distance_to_pivot_point / 20.0f;
        orbit_.MoveForward(-distance);
    }

    // Callback function for motion events
    void Application::OnMouseMove(glm::vec2 const& position)
    {
        // Compute mouse motion
        auto mouse_motion = position - MouseState::Get().previous_position_;
        MouseState::Get().previous_position_ = position;
        if (!MouseState::Get().pressed_buttons_)
            return;

        // Evaluate camera animation
        auto distance_to_pivot_point = glm::length(orbit_.Eye() - orbit_.Center());
        auto distance_x = mouse_motion.x * distance_to_pivot_point / 1500.0f;
        auto distance_y = mouse_motion.y * distance_to_pivot_point / 1500.0f;
        auto radians_x = mouse_motion.x / 500.0f;
        auto radians_y = mouse_motion.y / 500.0f;

        // Orbit the camera
        if (MouseState::Get().pressed_buttons_ & (1u << GLFW_MOUSE_BUTTON_LEFT))
        {
            orbit_.Rotate(radians_x, -radians_y);
        }
        if (MouseState::Get().pressed_buttons_ & (1u << GLFW_MOUSE_BUTTON_MIDDLE))
        {
            orbit_.MovePerpendicular(distance_x, -distance_y);
        }
        if (MouseState::Get().pressed_buttons_ & (1u << GLFW_MOUSE_BUTTON_RIGHT))
        {
            orbit_.MoveForward(-2.0f * distance_y);
        }
    }

    // Callback function for input events
    void Application::OnMousePress(int32_t button, int32_t action, int32_t)
    {
        auto mouse_flag = (1u << button);
        if (action == GLFW_PRESS)
        {
            MouseState::Get().pressed_buttons_ |= mouse_flag;
        }
        else
        {
            MouseState::Get().pressed_buttons_ &= ~mouse_flag;
        }
    }
}