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
            , pt_rays_pipeline_(manager_)
            , pt_rays_resolve_pipeline_(manager_)
        {
        }
        void Init(uint32_t num_rays, std::vector<float> const& vertices, std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes,
                  VkDeviceSize scratch_trace_size)
        {
            fence_ = manager_->CreateFence();

            VkDeviceSize params_size = VkDeviceSize(sizeof(Params));
            VkDeviceSize indices_size = VkDeviceSize(indices.size() * sizeof(uint32_t));
            VkDeviceSize vertices_size = VkDeviceSize(vertices.size() * sizeof(float));
            VkDeviceSize shapes_size = VkDeviceSize(shapes.size() * sizeof(Shape));
            VkDeviceSize random_size = VkDeviceSize(num_rays * sizeof(uint32_t));

            color_size_ = VkDeviceSize(num_rays * sizeof(uint32_t));
            VkDeviceSize camera_rays_size = VkDeviceSize(num_rays * sizeof(RrRay));
            VkDeviceSize ao_rays_size = VkDeviceSize(num_rays * sizeof(RrRay));

            VkDeviceSize ao_count_size = VkDeviceSize(4u * sizeof(uint32_t));
            VkDeviceSize hits_size = VkDeviceSize(num_rays * sizeof(RrHit));
            VkDeviceSize ao_size = VkDeviceSize(num_rays * sizeof(glm::uvec2));
            VkDeviceSize ao_id_size = VkDeviceSize(num_rays * sizeof(uint32_t));

            params_ = manager_->CreateBuffer(params_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            indices_ = manager_->CreateBuffer(indices_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            vertices_ = manager_->CreateBuffer(vertices_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            shapes_ = manager_->CreateBuffer(shapes_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            random_ = manager_->CreateBuffer(random_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            color_ = manager_->CreateBuffer(color_size_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            camera_rays_ = manager_->CreateBuffer(camera_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_rays_ = manager_->CreateBuffer(ao_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_count_ = manager_->CreateBuffer(ao_count_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            hits_ = manager_->CreateBuffer(hits_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_ = manager_->CreateBuffer(ao_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_id_ = manager_->CreateBuffer(ao_id_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            scratch_trace_ = manager_->CreateBuffer(scratch_trace_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

            InitMemoryForBuffers(num_rays, vertices, indices, shapes);
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
                                  std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes)
        {
            VkDeviceSize indices_offset = GetBufferMemorySize(params_);
            VkDeviceSize vertices_offset = indices_offset + GetBufferMemorySize(indices_);
            VkDeviceSize shapes_offset = vertices_offset + GetBufferMemorySize(vertices_);
            VkDeviceSize random_offset = shapes_offset + GetBufferMemorySize(shapes_);
            color_offset_ = random_offset + GetBufferMemorySize(random_);
            VkDeviceSize camera_rays_offset = color_offset_ + GetBufferMemorySize(color_);

            VkDeviceSize ao_rays_offset = camera_rays_offset + GetBufferMemorySize(camera_rays_);
            VkDeviceSize ao_count_offset = ao_rays_offset + GetBufferMemorySize(ao_rays_);
            VkDeviceSize hits_offset = ao_count_offset + GetBufferMemorySize(ao_count_);
            VkDeviceSize ao_offset = hits_offset + GetBufferMemorySize(hits_);
            VkDeviceSize ao_id_offset = ao_offset + GetBufferMemorySize(ao_);
            VkDeviceSize scratch_offset = ao_id_offset + GetBufferMemorySize(ao_id_);

            VkDeviceSize overall_memory_size = scratch_offset + GetBufferMemorySize(scratch_trace_);

            auto memory_index = manager_->FindDeviceMemoryIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            memory_ = manager_->AllocateDeviceMemory(memory_index, overall_memory_size);

            manager_->BindBufferMemory(params_.get(), memory_.get(), 0u);
            manager_->BindBufferMemory(indices_.get(), memory_.get(), indices_offset);
            manager_->BindBufferMemory(vertices_.get(), memory_.get(), vertices_offset);
            manager_->BindBufferMemory(shapes_.get(), memory_.get(), shapes_offset);
            manager_->BindBufferMemory(random_.get(), memory_.get(), random_offset);
            manager_->BindBufferMemory(color_.get(), memory_.get(), color_offset_);
            manager_->BindBufferMemory(camera_rays_.get(), memory_.get(), camera_rays_offset);
            manager_->BindBufferMemory(ao_rays_.get(), memory_.get(), ao_rays_offset);
            manager_->BindBufferMemory(ao_count_.get(), memory_.get(), ao_count_offset);
            manager_->BindBufferMemory(hits_.get(), memory_.get(), hits_offset);
            manager_->BindBufferMemory(ao_.get(), memory_.get(), ao_offset);
            manager_->BindBufferMemory(ao_id_.get(), memory_.get(), ao_id_offset);
            manager_->BindBufferMemory(scratch_trace_.get(), memory_.get(), scratch_offset);

            void* indices_ptr = manager_->MapMemory(memory_.get(), indices_offset, indices.size() * sizeof(uint32_t));
            memcpy(indices_ptr, indices.data(), indices.size() * sizeof(uint32_t));
            manager_->UnmapMemory(memory_.get(), indices_offset, indices.size() * sizeof(uint32_t));

            void* vertices_ptr = manager_->MapMemory(memory_.get(), vertices_offset, vertices.size() * sizeof(float));
            memcpy(vertices_ptr, vertices.data(), vertices.size() * sizeof(float));
            manager_->UnmapMemory(memory_.get(), vertices_offset, vertices.size() * sizeof(float));

            void* shapes_ptr = manager_->MapMemory(memory_.get(), shapes_offset, shapes.size() * sizeof(Shape));
            memcpy(shapes_ptr, shapes.data(), shapes.size() * sizeof(Shape));
            manager_->UnmapMemory(memory_.get(), shapes_offset, shapes.size() * sizeof(Shape));

            void* random_ptr = manager_->MapMemory(memory_.get(), random_offset, num_rays * sizeof(uint32_t));
            std::random_device dev;
            std::mt19937 rng(dev());
            std::uniform_int_distribution<std::mt19937::result_type> dist6(1, num_rays);

            uint32_t* random_data = (uint32_t*)random_ptr;
            for (uint32_t i = 0; i < num_rays; ++i)
            {
                random_data[i] = dist6(rng);
            }
            manager_->UnmapMemory(memory_.get(), random_offset, num_rays * sizeof(uint32_t));
        }

        // vulkan manager
        std::shared_ptr<VulkanManager> manager_;
        // ao buffers
        VkScopedObject<VkDeviceMemory> memory_;
        VkScopedObject<VkBuffer> indices_;
        VkScopedObject<VkBuffer> vertices_;
        VkScopedObject<VkBuffer> shapes_;
        VkScopedObject<VkBuffer> color_;
        VkScopedObject<VkBuffer> params_;
        VkScopedObject<VkBuffer> scratch_trace_;
        VkScopedObject<VkBuffer> camera_rays_;
        VkScopedObject<VkBuffer> ao_rays_;
        VkScopedObject<VkBuffer> ao_count_;
        VkScopedObject<VkBuffer> hits_;
        VkScopedObject<VkBuffer> random_;
        VkScopedObject<VkBuffer> ao_;
        VkScopedObject<VkBuffer> ao_id_;

        VkDeviceSize color_offset_;
        VkDeviceSize color_size_;

        // The fence to be signalled
        VkScopedObject<VkFence> fence_;
        // pipelines
        CommandBuffer ao_command_buffer_;
        // Pipelines
        // The pipeline object for generating the camera rays
        Pipeline camera_rays_pipeline_;
        // The pipeline object for generating the ambient occlusion rays
        Pipeline pt_rays_pipeline_;
        // The pipeline object for resolving the ambient occlusion rays
        Pipeline pt_rays_resolve_pipeline_;
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
        impl_->pt_rays_pipeline_.Create("shaders/ao_rays.comp.spv", {
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
        impl_->pt_rays_resolve_pipeline_.Create("shaders/ao_rays_resolve.comp.spv", {
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
            impl_->params_.get(),
            impl_->camera_rays_.get(),
            impl_->ao_count_.get(),
            impl_->ao_.get(),
            impl_->color_.get(),
            impl_->random_.get(),
        };
        VkDescriptorSet cam_rays_desc = impl_->camera_rays_pipeline_.Bind(cam_rays_bindings);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->camera_rays_pipeline_.GetPipelineLayout(), 0, 1, &cam_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(impl_->camera_rays_.get(), VK_ACCESS_SHADER_WRITE_BIT,
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
            impl_->camera_rays_.get(),
            impl_->hits_.get(),
            impl_->scratch_trace_.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays failed");
        }

        VkBuffer after_camera_rays_trace[2] = { impl_->hits_.get() , impl_->ao_count_.get() };
        manager_->EncodeBufferBarriers(after_camera_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Generate the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->pt_rays_pipeline_.GetPipeline());
        Binding ao_ray_all_binding[] =
        {
            impl_->params_.get(),
            impl_->ao_id_.get(),
            impl_->ao_rays_.get(),
            impl_->ao_count_.get(),
            impl_->hits_.get(),
            impl_->camera_rays_.get(),
            impl_->random_.get(),
            impl_->shapes_.get(),
            impl_->indices_.get(),
            impl_->vertices_.get()
        };

        VkDescriptorSet ao_rays_desc = impl_->pt_rays_pipeline_.Bind(ao_ray_all_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->pt_rays_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        VkBuffer before_ao_rays_trace[2] = { impl_->ao_rays_.get() , impl_->ao_count_.get() };
        manager_->EncodeBufferBarriers(before_ao_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Trace the ambient occlusion rays
        // it is not necessary to trace full hit
        status = rrCmdTraceRaysIndirect(
            context_,
            top_level_structure_,
            RR_QUERY_TYPE_INTERSECT,
            RR_OUTPUT_TYPE_FULL_HIT,
            0u,
            impl_->ao_rays_.get(),
            impl_->hits_.get(),
            impl_->ao_count_.get(),
            impl_->scratch_trace_.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays Indirect failed");
        }

        manager_->EncodeBufferBarrier(impl_->hits_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Resolve the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->pt_rays_resolve_pipeline_.GetPipeline());
        Binding ao_color_ray_binding[] =
        {
            impl_->ao_.get(),
            impl_->color_.get(),
            impl_->ao_id_.get(),
            impl_->ao_count_.get(),
            impl_->hits_.get(),
        };
        VkDescriptorSet ao_rays_resolve_desc = impl_->pt_rays_resolve_pipeline_.Bind(ao_color_ray_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->pt_rays_resolve_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_resolve_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(impl_->color_.get(), VK_ACCESS_SHADER_WRITE_BIT,
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
        void* params_ptr = manager_->MapMemory(impl_->memory_.get(), 0u, sizeof(Params));
        impl_->WaitForFence();
        impl_->ResetFence();

        // Transfer the shader parameters to device memory
        memcpy(params_ptr, &params, sizeof(Params));
        manager_->UnmapMemory(impl_->memory_.get(), 0u, sizeof(Params));
    }
    std::vector<uint32_t> Ao::GetColor() const
    {
        std::vector<uint32_t> color(impl_->color_size_ / sizeof(uint32_t));
        void* color_ptr = manager_->MapMemory(impl_->memory_.get(), impl_->color_offset_, impl_->color_size_);
        impl_->WaitForFence();

        memcpy(color.data(), color_ptr, impl_->color_size_);
        manager_->UnmapMemory(impl_->memory_.get(), impl_->color_offset_, impl_->color_size_);

        return color;
    }

    VkBuffer Ao::GetColorBuffer() const
    {
        return impl_->color_.get();
    }

    void Ao::SetColor(std::vector<uint32_t> const& color)
    {
        impl_->WaitForFence();
        void* color_ptr = manager_->MapMemory(impl_->memory_.get(), impl_->color_offset_, impl_->color_size_);
        memcpy(color_ptr, color.data(), impl_->color_size_);
        manager_->UnmapMemory(impl_->memory_.get(), impl_->color_offset_, impl_->color_size_);
    }
}