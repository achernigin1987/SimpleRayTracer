#include "PathTracerImpl.h"
#include <iterator>
#include <glm/glm.hpp>

namespace PathTracer
{
    PathTracerImpl::PathTracerImpl(std::shared_ptr<VulkanManager> manager)
        : manager_ (manager)
        , ao_command_buffer_(manager_->command_pool_, manager_->device_)
        , camera_rays_pipeline_(manager_)
        , ao_rays_pipeline_(manager_)
        , ao_rays_resolve_pipeline_(manager_)
    {}

    void PathTracerImpl::Init(Scene const& scene, RrAccelerationStructure top_level_structure, RrContext context, uint32_t num_rays)
    {
        top_level_structure_ = top_level_structure;
        context_ = context;
        

        fence_ = CreateFence();

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

            std::copy(mesh.Indices(), mesh.Indices() + mesh.GetIndexCount(), std::back_inserter(indices));
            std::copy(mesh.Vertices(), mesh.Vertices() + mesh.GetIndexCount(), std::back_inserter(vertices));

            base_vertex += mesh.GetVertexCount();
            first_index += mesh.GetIndexCount();
        }

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

        VkMemoryRequirements accel_trace_mem_reqs;
        rrGetAccelerationStructureTraceScratchMemoryRequirements(context_, top_level_structure, num_rays, &accel_trace_mem_reqs);
        VkDeviceSize scratch_trace_size = accel_trace_mem_reqs.size;

        VkDeviceSize overall_memory_size = indices_size + vertices_size + shapes_size + color_size +
            camera_rays_size + ao_rays_size + ao_count_size + hits_size +
            random_size + ao_size + ao_id_size + params_size + scratch_trace_size;

        auto memory_index = manager_->FindDeviceMemoryIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        memory_ = manager_->AllocateDeviceMemory(memory_index, overall_memory_size);

        params_ = manager_->CreateBuffer(params_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        indices_ = manager_->CreateBuffer(indices_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        vertices_ = manager_->CreateBuffer(vertices_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        shapes_ = manager_->CreateBuffer(shapes_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        random_ = manager_->CreateBuffer(random_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        color_ = manager_->CreateBuffer(color_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        camera_rays_ = manager_->CreateBuffer(camera_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ao_rays_ = manager_->CreateBuffer(ao_rays_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ao_count_ = manager_->CreateBuffer(ao_count_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        hits_ = manager_->CreateBuffer(hits_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ao_ = manager_->CreateBuffer(ao_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ao_id_ = manager_->CreateBuffer(ao_id_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        scratch_trace_ = manager_->CreateBuffer(scratch_trace_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        manager_->BindBufferMemory(params_.get(), memory_.get(), 0u);
        manager_->BindBufferMemory(indices_.get(), memory_.get(), sizeof(Params));
        manager_->BindBufferMemory(vertices_.get(), memory_.get(), sizeof(Params) + indices_size);
        manager_->BindBufferMemory(shapes_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size);
        manager_->BindBufferMemory(random_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size);
        manager_->BindBufferMemory(color_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size
                                   + shapes_size + random_size);
        manager_->BindBufferMemory(camera_rays_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size
                                   + shapes_size + random_size + color_size);
        manager_->BindBufferMemory(ao_rays_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size);
        manager_->BindBufferMemory(ao_count_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size + ao_rays_size);
        manager_->BindBufferMemory(hits_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size + ao_rays_size + ao_count_size);
        manager_->BindBufferMemory(ao_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size + ao_rays_size + ao_count_size + hits_size);
        manager_->BindBufferMemory(ao_id_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size + ao_rays_size + ao_count_size + hits_size + ao_size);
        manager_->BindBufferMemory(scratch_trace_.get(), memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size
                                   + random_size + color_size + camera_rays_size + ao_rays_size + ao_count_size + hits_size + ao_size + ao_id_size);

        void* indices_ptr = manager_->MapMemory(memory_.get(), sizeof(Params), indices_size);
        memcpy(indices_ptr, indices.data(), indices_size);
        manager_->UnmapMemory(memory_.get(), sizeof(Params), indices_size);

        void* vertices_ptr = manager_->MapMemory(memory_.get(), sizeof(Params) + indices_size, vertices_size);
        memcpy(vertices_ptr, vertices.data(), vertices_size);
        manager_->UnmapMemory(memory_.get(), sizeof(Params) + indices_size, vertices_size);

        void* shapes_ptr = manager_->MapMemory(memory_.get(), sizeof(Params) + indices_size + vertices_size, shapes_size);
        memcpy(shapes_ptr, shapes.data(), shapes_size);
        manager_->UnmapMemory(memory_.get(), sizeof(Params) + indices_size + vertices_size, vertices_size);

        void* random_ptr = manager_->MapMemory(memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size, random_size);
        uint32_t* random_data = (uint32_t*)random_ptr;
        for (uint32_t i = 0; i < num_rays; ++i)
        {
            random_data[i] = std::rand() + 3;
        }
        manager_->UnmapMemory(memory_.get(), sizeof(Params) + indices_size + vertices_size + shapes_size, random_size);
        PrepareCommandBuffer(num_rays);
    }

    VkScopedObject<VkFence> PathTracerImpl::CreateFence() const
    {
        VkFence fence;
        // Create the fence
        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkResult result = vkCreateFence(manager_->device_, &fence_create_info, nullptr, &fence);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create fence");
        }

        return VkScopedObject<VkFence>(fence, [this](VkFence fence)
        {
            vkDestroyFence(manager_->device_, fence, nullptr);
        });
    }

    void PathTracerImpl::PrepareCommandBuffer(uint32_t num_rays)
    {
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
        vkCmdBindPipeline(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, camera_rays_pipeline_.GetPipeline());
        // Generate the camera rays
        Binding cam_rays_bindings[] =
        {
            params_.get(),
            camera_rays_.get(),
            ao_count_.get(),
            ao_.get(),
            color_.get(),
            random_.get(),
        };
        VkDescriptorSet cam_rays_desc = camera_rays_pipeline_.Bind(cam_rays_bindings);
        vkCmdBindDescriptorSets(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, camera_rays_pipeline_.GetPipelineLayout(), 0, 1, &cam_rays_desc, 0, nullptr);
        vkCmdDispatch(ao_command_buffer_, (num_rays + 63) / 64, 1u, 1u);

        VkCommandBuffer cmd_buf = ao_command_buffer_.Get();
        manager_->EncodeBufferBarrier(camera_rays_.get(), VK_ACCESS_SHADER_WRITE_BIT,
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
            camera_rays_.get(),
            hits_.get(),
            scratch_trace_.get(),
            ao_command_buffer_);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays failed");
        }

        manager_->EncodeBufferBarrier(camera_rays_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);
        VkBuffer after_camera_rays_trace[2] = { hits_.get() , ao_count_.get() };
        manager_->EncodeBufferBarriers(after_camera_rays_trace, 2, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Generate the ambient occlusion rays
        vkCmdBindPipeline(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_pipeline_.GetPipeline());
        Binding ao_ray_all_binding[] =
        {
            params_.get(),
            ao_id_.get(),
            ao_rays_.get(),
            ao_count_.get(),
            hits_.get(),
            camera_rays_.get(),
            random_.get(),
            shapes_.get(),
            indices_.get(),
            vertices_.get()
        };

        VkDescriptorSet ao_rays_desc = camera_rays_pipeline_.Bind(ao_ray_all_binding);
        vkCmdBindDescriptorSets(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_desc, 0, nullptr);
        vkCmdDispatch(ao_command_buffer_, (num_rays + 63) / 64, 1u, 1u);

        VkBuffer before_ao_rays_trace[2] = { ao_rays_.get() , ao_count_.get() };
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
            ao_rays_.get(),
            hits_.get(),
            ao_count_.get(),
            scratch_trace_.get(),
            ao_command_buffer_);

        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Trace Rays Indirect failed");
        }

        manager_->EncodeBufferBarrier(hits_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        // Resolve the ambient occlusion rays
        vkCmdBindPipeline(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_resolve_pipeline_.GetPipeline());
        Binding ao_color_ray_binding[] =
        {
            ao_.get(),
            color_.get(),
            ao_id_.get(),
            ao_count_.get(),
            hits_.get()
        };
        VkDescriptorSet ao_rays_resolve_desc = ao_rays_resolve_pipeline_.Bind(ao_color_ray_binding);
        vkCmdBindDescriptorSets(ao_command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, ao_rays_resolve_pipeline_.GetPipelineLayout(), 0, 1, &ao_rays_resolve_desc, 0, nullptr);
        vkCmdDispatch(ao_command_buffer_, (num_rays + 63) / 64, 1u, 1u);

        manager_->EncodeBufferBarrier(color_.get(), VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_buf);

        ao_command_buffer_.End();
    }

    VkResult PathTracerImpl::Submit()
    {
        return ao_command_buffer_.Submit(manager_->queue_, nullptr, 0u, nullptr, 0u, fence_.get());
    }

    void PathTracerImpl::UpdateView(Params const & params)
    {
        void* params_ptr = manager_->MapMemory(memory_.get(), 0u, sizeof(Params));
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
        vkResetFences(manager_->device_, 1, &fence);

        // Transfer the shader parameters to device memory
        memcpy(params_ptr, &params, sizeof(Params));
        manager_->UnmapMemory(memory_.get(), 0u, sizeof(Params));
    }
}