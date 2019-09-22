#include "AoSample.h"
#include <iterator>
#include <random>
#include <glm/glm.hpp>

namespace PathTracer
{
    constexpr auto const kMaxTextures = 512u;

    struct Ao::AoImpl
    {
        AoImpl(std::shared_ptr<VulkanManager> manager)
            : manager_(manager)
            , ao_command_buffer_(manager_->command_pool_, manager_->device_)
            , camera_rays_pipeline_(manager_)
            , ao_rays_pipeline_(manager_)
            , ao_rays_resolve_pipeline_(manager_)
        {
        }
        void Init(uint32_t num_rays, std::vector<float> const& vertices, std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes,
                  VkDeviceSize scratch_trace_size)
        {
            fence_ = manager_->CreateFence();

            InitMemoryForBuffers(num_rays, vertices, indices, shapes, scratch_trace_size);
        }

        VkDeviceSize GetBufferMemorySize(VkScopedObject<VkBuffer> buffer)
        {
            auto mem_req = manager_->GetBufferMemoryRequirements(buffer.get());
            VkDeviceSize offset = VulkanManager::align(mem_req.size, mem_req.alignment);
            return offset;
        }

        void WaitForFence()
        {
            VkFence fence = fence_.get();
            if (vkGetFenceStatus(manager_->device_, fence) == VK_NOT_READY)
            {
                auto timeout = 1000000000ull;   // 1 sec
                for (;;)
                {
                    if (vkWaitForFences(manager_->device_, 1, &fence, VK_TRUE, timeout) == VK_SUCCESS)
                    {
                        break;
                    }
                    std::cout << "Performance warning: Waiting for fence to be signalled" << std::endl;
                }
            }
        }
        void ResetFence()
        {
            VkFence fence = fence_.get();
            vkResetFences(manager_->device_, 1, &fence);
        }

        void InitMemoryForBuffers(uint32_t num_rays, std::vector<float> const& vertices,
                                  std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes, VkDeviceSize scratch_trace_size)
        {
            indices_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indices);
            vertices_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertices);
            shapes_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shapes);

            std::random_device dev;
            std::mt19937 rng(dev());
            std::uniform_int_distribution<std::mt19937::result_type> dist6(1, num_rays);

            std::vector<uint32_t> random_data(num_rays);
            for (uint32_t i = 0; i < num_rays; ++i)
            {
                random_data[i] = dist6(rng);
            }
            random_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, random_data);

            std::vector<uint32_t> initializer(num_rays, 0u);
            color_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, initializer);
            color_size_ = initializer.size() * sizeof(uint32_t);
            ao_count_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, initializer);
            shadow_hits_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, initializer);
            ao_id_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, initializer);

            std::vector<RrRay> rays_initializer(num_rays);
            ao_rays_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rays_initializer);
            camera_rays_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rays_initializer);

            std::vector<RrHit> hits_initializer(num_rays);
            hits_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, hits_initializer);

            std::vector<uint8_t> scratch_initializer(scratch_trace_size);
            scratch_trace_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratch_initializer);

            std::vector<glm::uvec2> ao_initializer(num_rays);
            ao_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ao_initializer);

            std::vector<Params> params_initializer(1);
            params_ = manager_->CreateAllocatedBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, params_initializer);
        }

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

        VkDeviceSize color_size_;

        // The fence to be signalled
        VkScopedObject<VkFence> fence_;
        // pipelines
        CommandBuffer ao_command_buffer_;
        // Pipelines
        // The pipeline object for generating the camera rays
        Pipeline camera_rays_pipeline_;
        // The pipeline object for generating the ambient occlusion rays
        Pipeline ao_rays_pipeline_;
        // The pipeline object for resolving the ambient occlusion rays
        Pipeline ao_rays_resolve_pipeline_;
    };

    Ao::Ao(std::shared_ptr<VulkanManager> manager)
        : manager_(manager)
        , impl_(std::make_unique<AoImpl>(manager))
    {
    }

    Ao::~Ao()
    {
    }

    void Ao::Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays)
    {
        top_level_structure_ = top_level_structure;
        context_ = context;

        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<Shape> shapes(scene.GetMeshCount());
        uint32_t base_vertex = 0u, first_index = 0u;

        for (size_t i = 0; i < scene.GetMeshCount(); i++)
        {
            auto& shape = shapes[i];
            auto const& mesh = scene.meshes_[i];

            shape.count_ = mesh.GetIndexCount();
            shape.base_vertex_ = base_vertex;
            shape.first_index_ = first_index;
            shape.material_id = mesh.Material();

            std::copy(mesh.Indices(), mesh.Indices() + mesh.GetIndexSize(), std::back_inserter(indices));
            std::copy(mesh.Vertices(), mesh.Vertices() + mesh.GetVertexSize(), std::back_inserter(vertices));

            base_vertex += mesh.GetVertexCount();
            first_index += mesh.GetIndexCount();
        }

        VkMemoryRequirements accel_trace_mem_reqs;
        rrGetAccelerationStructureTraceScratchMemoryRequirements(context_, top_level_structure, num_rays, &accel_trace_mem_reqs);
        VkDeviceSize scratch_trace_size = accel_trace_mem_reqs.size;
        impl_->Init(num_rays, vertices, indices, shapes, scratch_trace_size);

        PrepareCommandBuffer(num_rays);
    }

    void Ao::PrepareCommandBuffer(uint32_t num_rays)
    {
        VkCommandBuffer cmd_buf = impl_->ao_command_buffer_.Get();
        // Create the compute pipelines
        impl_->camera_rays_pipeline_.Create("shaders/camera_rays.comp.spv", {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},  // Params
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Rays
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // RayCount
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // AoBuffer
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Color
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}   // Random
                                            });
        impl_->ao_rays_pipeline_.Create("shaders/ao_rays.comp.spv", {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},  // Params
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Ids
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Rays
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // RayCount
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Hits
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // CameraRays
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Random
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Shapes
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Indices
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}  // Vertices
                                        });
        impl_->ao_rays_resolve_pipeline_.Create("shaders/ao_rays_resolve.comp.spv", {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // AoBuffer
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Color
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // Ids
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // RayCount
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}   // Hits
                                                });

        impl_->ao_command_buffer_.Begin();
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->camera_rays_pipeline_.GetPipeline());
        // Generate the camera rays
        Binding cam_rays_bindings[] =
        {
            impl_->params_.first.get(),
            impl_->camera_rays_.first.get(),
            impl_->ao_count_.first.get(),
            impl_->ao_.first.get(),
            impl_->color_.first.get(),
            impl_->random_.first.get(),
        };
        VkDescriptorSet cam_rays_desc = impl_->camera_rays_pipeline_.Bind(cam_rays_bindings);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->camera_rays_pipeline_.GetPipelineLayout(), 0, 1, &cam_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(impl_->camera_rays_.first.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Trace the camera rays
        auto status = rrCmdTraceRays(
            context_,
            top_level_structure_,
            RR_QUERY_TYPE_INTERSECT,
            RR_OUTPUT_TYPE_FULL_HIT,
            0u,
            num_rays,
            impl_->camera_rays_.first.get(),
            impl_->hits_.first.get(),
            impl_->scratch_trace_.first.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays failed");
        }

        VkBuffer after_camera_rays_trace[2] = { impl_->hits_.first.get() , impl_->ao_count_.first.get() };
        manager_->EncodeBufferBarriers(after_camera_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Generate the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->ao_rays_pipeline_.GetPipeline());
        Binding ao_ray_all_binding[] =
        {
            impl_->params_.first.get(),
            impl_->ao_id_.first.get(),
            impl_->ao_rays_.first.get(),
            impl_->ao_count_.first.get(),
            impl_->hits_.first.get(),
            impl_->camera_rays_.first.get(),
            impl_->random_.first.get(),
            impl_->shapes_.first.get(),
            impl_->indices_.first.get(),
            impl_->vertices_.first.get()
        };

        VkDescriptorSet ao_rays_desc = impl_->ao_rays_pipeline_.Bind(ao_ray_all_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->ao_rays_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        VkBuffer before_ao_rays_trace[2] = { impl_->ao_rays_.first.get() , impl_->ao_count_.first.get() };
        manager_->EncodeBufferBarriers(before_ao_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        status = rrCmdTraceRaysIndirect(
            context_,
            top_level_structure_,
            RR_QUERY_TYPE_OCCLUDED,
            RR_OUTPUT_TYPE_INSTANCE_ID_ONLY,
            0u,
            impl_->ao_rays_.first.get(),
            impl_->shadow_hits_.first.get(),
            impl_->ao_count_.first.get(),
            impl_->scratch_trace_.first.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays Indirect failed");
        }

        manager_->EncodeBufferBarrier(impl_->shadow_hits_.first.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Resolve the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->ao_rays_resolve_pipeline_.GetPipeline());
        Binding ao_color_ray_binding[] =
        {
            impl_->ao_.first.get(),
            impl_->color_.first.get(),
            impl_->ao_id_.first.get(),
            impl_->ao_count_.first.get(),
            impl_->shadow_hits_.first.get(),
        };
        VkDescriptorSet ao_rays_resolve_desc = impl_->ao_rays_resolve_pipeline_.Bind(ao_color_ray_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->ao_rays_resolve_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_resolve_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(impl_->color_.first.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, cmd_buf);

        impl_->ao_command_buffer_.End();
    }

    VkResult Ao::Submit()
    {
        return impl_->ao_command_buffer_.Submit(manager_->queue_, nullptr, 0u, nullptr, 0u, impl_->fence_.get());
    }

    void Ao::UpdateView(Params const & params)
    {
        void* params_ptr = manager_->MapMemory(impl_->params_.second.get(), 0u, sizeof(Params));
        impl_->WaitForFence();
        impl_->ResetFence();

        // Transfer the shader parameters to device memory
        memcpy(params_ptr, &params, sizeof(Params));
        manager_->UnmapMemory(impl_->params_.second.get(), 0u, sizeof(Params));
    }
    std::vector<uint32_t> Ao::GetColor() const
    {
        std::vector<uint32_t> color(impl_->color_size_ / sizeof(uint32_t));
        void* color_ptr = manager_->MapMemory(impl_->color_.second.get(), 0u, impl_->color_size_);
        impl_->WaitForFence();

        memcpy(color.data(), color_ptr, impl_->color_size_);
        manager_->UnmapMemory(impl_->color_.second.get(), 0u, impl_->color_size_);

        return color;
    }

    VkBuffer Ao::GetColorBuffer() const
    {
        return impl_->color_.first.get();
    }

    void Ao::SetColor(std::vector<uint32_t> const& color)
    {
        impl_->WaitForFence();
        void* color_ptr = manager_->MapMemory(impl_->color_.second.get(), 0u, impl_->color_size_);
        memcpy(color_ptr, color.data(), impl_->color_size_);
        manager_->UnmapMemory(impl_->color_.second.get(), 0u, impl_->color_size_);
    }
}