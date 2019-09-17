#include "PathTracerImpl.h"
#include <iterator>
#include <random>
#include <glm/glm.hpp>

namespace PathTracer
{
    struct PathTracerImpl::PathTraceImplH
    {
        PathTraceImplH(std::shared_ptr<VulkanManager> manager)
            : manager_(manager)
        {}
        void Init(uint32_t num_rays, std::vector<float> const& vertices, std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes, VkDeviceSize scratch_trace_size)
        {
            fence_ = manager_->CreateFence();

            VkDeviceSize params_size = VkDeviceSize(sizeof(Params));
            VkDeviceSize indices_size = VkDeviceSize(indices.size() * sizeof(uint32_t));
            VkDeviceSize vertices_size = VkDeviceSize(vertices.size() * sizeof(float));
            VkDeviceSize shapes_size = VkDeviceSize(shapes.size() * sizeof(Shape));
            VkDeviceSize random_size = VkDeviceSize(num_rays * sizeof(uint32_t));

            VkDeviceSize color_size = VkDeviceSize(num_rays * sizeof(uint32_t));
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
            color_ = manager_->CreateBuffer(color_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            camera_rays_ = manager_->CreateBuffer(camera_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_rays_ = manager_->CreateBuffer(ao_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_count_ = manager_->CreateBuffer(ao_count_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            hits_ = manager_->CreateBuffer(hits_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_ = manager_->CreateBuffer(ao_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            ao_id_ = manager_->CreateBuffer(ao_id_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            scratch_trace_ = manager_->CreateBuffer(scratch_trace_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

            InitMemoryForBuffers(num_rays,vertices, indices, shapes);
        }

        VkDeviceSize GetBufferMemorySize(VkScopedObject<VkBuffer> buffer)
        {
            auto mem_req = manager_->GetBufferMemoryRequirements(buffer.get());
            VkDeviceSize offset = VulkanManager::align(mem_req.size, mem_req.alignment);
            return offset;
        }

        void InitMemoryForBuffers(uint32_t num_rays, std::vector<float> const& vertices, std::vector<uint32_t> const& indices, std::vector<Shape> const& shapes)
        {
            VkDeviceSize indices_offset = GetBufferMemorySize(params_);
            VkDeviceSize vertices_offset = indices_offset + GetBufferMemorySize(indices_);
            VkDeviceSize shapes_offset = vertices_offset + GetBufferMemorySize(vertices_);
            VkDeviceSize random_offset = shapes_offset + GetBufferMemorySize(shapes_);
            VkDeviceSize color_offset = random_offset + GetBufferMemorySize(random_);
            VkDeviceSize camera_rays_offset = color_offset + GetBufferMemorySize(color_);

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
            manager_->BindBufferMemory(color_.get(), memory_.get(), color_offset);
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
        // The fence to be signalled
        VkScopedObject<VkFence> fence_;
        std::shared_ptr<VulkanManager> manager_;
    };

    PathTracerImpl::PathTracerImpl(std::shared_ptr<VulkanManager> manager)
        : manager_ (manager)
        , holder_(std::make_unique<PathTraceImplH>(manager))
        , ao_command_buffer_(manager_->command_pool_, manager_->device_)
        , camera_rays_pipeline_(manager_)
        , ao_rays_pipeline_(manager_)
        , ao_rays_resolve_pipeline_(manager_)
    {}

    PathTracerImpl::~PathTracerImpl()
    {}

    void PathTracerImpl::Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays)
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

            std::copy(mesh.Indices(), mesh.Indices() + mesh.GetIndexSize(), std::back_inserter(indices));
            std::copy(mesh.Vertices(), mesh.Vertices() + mesh.GetVertexSize(), std::back_inserter(vertices));

            base_vertex += mesh.GetVertexCount();
            first_index += mesh.GetIndexCount();
        }

        VkMemoryRequirements accel_trace_mem_reqs;
        rrGetAccelerationStructureTraceScratchMemoryRequirements(context_, top_level_structure, num_rays, &accel_trace_mem_reqs);
        VkDeviceSize scratch_trace_size = accel_trace_mem_reqs.size;
        holder_->Init(num_rays, vertices, indices, shapes, scratch_trace_size);

        PrepareCommandBuffer(num_rays);
    }

    void PathTracerImpl::PrepareCommandBuffer(uint32_t num_rays)
    {
        VkCommandBuffer cmd_buf = ao_command_buffer_.Get();
        // Create the compute pipelines
        camera_rays_pipeline_.Create("camera_rays.comp.spv", {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // Params
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Rays
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RayCount
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // AoBuffer
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Color
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER   // Random
                                     });
        ao_rays_pipeline_.Create("ao_rays.comp.spv", {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // Params
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Ids
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Rays
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RayCount
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Hits
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // CameraRays
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Random
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Shapes
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Indices
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER   // Vertices
                                 });
        ao_rays_resolve_pipeline_.Create("ao_rays_resolve.comp.spv", {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // AoBuffer
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Color
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // Ids
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RayCount
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER   // Hits
                                         });

        ao_command_buffer_.Begin();
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, camera_rays_pipeline_.GetPipeline());
        // Generate the camera rays
        Binding cam_rays_bindings[] =
        {
            holder_->params_.get(),
            holder_->camera_rays_.get(),
            holder_->ao_count_.get(),
            holder_->ao_.get(),
            holder_->color_.get(),
            holder_->random_.get(),
        };
        VkDescriptorSet cam_rays_desc = camera_rays_pipeline_.Bind(cam_rays_bindings);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, camera_rays_pipeline_.GetPipelineLayout(), 0, 1, &cam_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(holder_->camera_rays_.get(), VK_ACCESS_SHADER_WRITE_BIT,
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
            holder_->camera_rays_.get(),
            holder_->hits_.get(),
            holder_->scratch_trace_.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays failed");
        }

        VkBuffer after_camera_rays_trace[2] = { holder_->hits_.get() , holder_->ao_count_.get() };
        manager_->EncodeBufferBarriers(after_camera_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Generate the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_pipeline_.GetPipeline());
        Binding ao_ray_all_binding[] =
        {
            holder_->params_.get(),
            holder_->ao_id_.get(),
            holder_->ao_rays_.get(),
            holder_->ao_count_.get(),
            holder_->hits_.get(),
            holder_->camera_rays_.get(),
            holder_->random_.get(),
            holder_->shapes_.get(),
            holder_->indices_.get(),
            holder_->vertices_.get()
        };

        VkDescriptorSet ao_rays_desc = ao_rays_pipeline_.Bind(ao_ray_all_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        VkBuffer before_ao_rays_trace[2] = { holder_->ao_rays_.get() , holder_->ao_count_.get() };
        manager_->EncodeBufferBarriers(before_ao_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Trace the ambient occlusion rays
        status = rrCmdTraceRaysIndirect(
            context_,
            top_level_structure_,
            RR_QUERY_TYPE_INTERSECT,
            RR_OUTPUT_TYPE_FULL_HIT,    // TODO: no need for full hit info here (gboisse)
            0u,
            holder_->ao_rays_.get(),
            holder_->hits_.get(),
            holder_->ao_count_.get(),
            holder_->scratch_trace_.get(),
            cmd_buf);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays Indirect failed");
        }

        manager_->EncodeBufferBarrier(holder_->hits_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Resolve the ambient occlusion rays
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_resolve_pipeline_.GetPipeline());
        Binding ao_color_ray_binding[] =
        {
            holder_->ao_.get(),
            holder_->color_.get(),
            holder_->ao_id_.get(),
            holder_->ao_count_.get(),
            holder_->hits_.get()
        };
        VkDescriptorSet ao_rays_resolve_desc = ao_rays_resolve_pipeline_.Bind(ao_color_ray_binding);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_resolve_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_resolve_desc, 0, nullptr);
        vkCmdDispatch(cmd_buf, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(holder_->color_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, cmd_buf);

        ao_command_buffer_.End();
    }

    VkResult PathTracerImpl::Submit()
    {
        return ao_command_buffer_.Submit(manager_->queue_, nullptr, 0u, nullptr, 0u, holder_->fence_.get());
    }

    void PathTracerImpl::UpdateView(Params const & params)
    {
        void* params_ptr = manager_->MapMemory(holder_->memory_.get(), 0u, sizeof(Params));
        VkFence fence = holder_->fence_.get();
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
        vkResetFences(manager_->device_, 1, &fence);

        // Transfer the shader parameters to device memory
        memcpy(params_ptr, &params, sizeof(Params));
        manager_->UnmapMemory(holder_->memory_.get(), 0u, sizeof(Params));
    }
    VkBuffer PathTracerImpl::GetColor() const
    {
        return holder_->color_.get();
    }
}