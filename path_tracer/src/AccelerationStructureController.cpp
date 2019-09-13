#include "AccelerationStructureController.h"
#include "VulkanManager.h"
#include <algorithm>
#include <iostream>

namespace PathTracer
{
    AccelerationStructureController::AccelerationStructureController(std::shared_ptr<VulkanManager> vk_manager)
        : vulkan_manager_(vk_manager)
        , top_level_accel_(VK_NULL_HANDLE)
        , accel_buffer_(VK_NULL_HANDLE)
    {
        // Initialize RadeonRays
        RrApplicationInfo app_info = {};
        app_info.pApplicationName = "Sample";
        app_info.applicationVersion = 1;
        app_info.pEngineName = "RadeonRaysNext";
        app_info.engineVersion = 1;
        app_info.apiVersion = VK_API_VERSION_1_0;
        app_info.cachedDescriptorsNumber = 10u;

        RrContextCreateInfo context_info = { &app_info };
        auto status = rrCreateContext(vulkan_manager_->device_, vulkan_manager_->device_.physical_device_, &context_info, &context_);
        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot create context");
        }
    }
    AccelerationStructureController::~AccelerationStructureController()
    {
        // Terminate RadeonRays
        for (auto bottom_acc : bottom_level_accel_)
        {
            if (bottom_acc != VK_NULL_HANDLE)
            {
                rrDestroyAccelerationStructure(context_, bottom_acc);
            }
        }
        if (top_level_accel_ != VK_NULL_HANDLE)
        {
            rrDestroyAccelerationStructure(context_, top_level_accel_);
        }
        rrDestroyContext(context_);
    }

    bool AccelerationStructureController::BuildAccelerationStructure(Scene const& scene)
    {
        VkScopedObject<VkDeviceMemory> accel_build_buffer;

        // Create our acceleration structure
        RrAccelerationStructureCreateInfo create_info = {};
        create_info.type = RR_ACCELERATION_STRUCTURE_TYPE_SCENE;
        create_info.flags = 0u;
        create_info.maxPrims = scene.GetMeshCount();

        RrStatus status = rrCreateAccelerationStructure(context_, &create_info, &top_level_accel_);
        if (status != RR_STATUS_SUCCESS)
        {
            return false;
        }

        uint32_t base_vertex = 0u, first_index = 0u;
        std::vector<uint32_t> vertex_offset(scene.GetMeshCount());
        std::vector<uint32_t> index_offset(scene.GetMeshCount());
        for (size_t mesh_id = 0; mesh_id < scene.meshes_.size(); mesh_id++)
        {
            vertex_offset[mesh_id] = base_vertex;
            index_offset[mesh_id] = first_index;
        }

        // Collect memory requirements
        std::vector<VkMemoryRequirements> meshes_mem_reqs;
        std::vector<VkMemoryRequirements> meshes_scratch_mem_reqs;

        for (uint32_t i = 0u; i < scene.GetMeshCount(); i++)
        {
            const auto& mesh = scene.meshes_[i];
            RrAccelerationStructureCreateInfo info;
            info.type = RR_ACCELERATION_STRUCTURE_TYPE_MESH;
            info.maxPrims = mesh.GetPrimCount();
            info.flags = 0u;

            RrAccelerationStructure mesh_acc_structure = nullptr;
            auto res = rrCreateAccelerationStructure(context_, &info, &mesh_acc_structure);
            if (res != RR_STATUS_SUCCESS)
            {
                throw std::runtime_error("Cannot create acceleration structure");
            }

            VkMemoryRequirements mesh_mem_reqs;
            VkMemoryRequirements mesh_scratch_mem_reqs;

            res = rrGetAccelerationStructureMemoryRequirements(context_, mesh_acc_structure, &mesh_mem_reqs);
            if (res != RR_STATUS_SUCCESS)
            {
                throw std::runtime_error("Cannot get acceleration structure memory requirements");
            }

            res = rrGetAccelerationStructureBuildScratchMemoryRequirements(context_, mesh_acc_structure, &mesh_scratch_mem_reqs);
            if (res != RR_STATUS_SUCCESS)
            {
                throw std::runtime_error("Cannot get acceleration structure build scratch memory");
            }

            bottom_level_accel_.push_back(mesh_acc_structure);
            meshes_mem_reqs.push_back(mesh_mem_reqs);
            meshes_scratch_mem_reqs.push_back(mesh_scratch_mem_reqs);
        }

        VkMemoryRequirements scene_mem_reqs;
        VkMemoryRequirements scene_scratch_mem_reqs;
        auto res = rrGetAccelerationStructureMemoryRequirements(context_, top_level_accel_, &scene_mem_reqs);
        if (res != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot get acceleration structure memory requirements");
        }
        res = rrGetAccelerationStructureBuildScratchMemoryRequirements(context_, top_level_accel_, &scene_scratch_mem_reqs);
        if (res != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot get acceleration structure build scratch memory");
        }

        VkDeviceSize required_mem_size = 0;
        VkDeviceSize required_scratch_mem_size = 0;
        for (auto i = 0u; i < scene.GetMeshCount(); ++i)
        {
            required_mem_size += meshes_mem_reqs[i].size;

            required_scratch_mem_size = std::max(meshes_scratch_mem_reqs[i].size, required_scratch_mem_size);
        }

        required_mem_size += scene_mem_reqs.size;
        required_scratch_mem_size = std::max(required_scratch_mem_size, scene_scratch_mem_reqs.size);

        std::cout << "Top-level acceleration structure build part size: " << required_scratch_mem_size * 1e-9 << " Gb" << std::endl;
        std::cout << "Top-level acceleration structure part size: " << required_mem_size * 1e-9 << " Gb" << std::endl;

        auto scratch_memory_index = vulkan_manager_->FindDeviceMemoryIndex(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        accel_build_buffer = vulkan_manager_->AllocateDeviceMemory(scratch_memory_index, required_scratch_mem_size);

        auto local_memory_index = vulkan_manager_->FindDeviceMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        accel_buffer_ = vulkan_manager_->AllocateDeviceMemory(local_memory_index, required_mem_size);


        // Init top-level acceleration structure
        std::vector<RrInstanceBuildInfo> build_infos(scene.GetMeshCount());
        std::vector<float> instance_transforms(scene.GetMeshCount() * 12);

        // Now bind memory for all meshes.
        auto offset = 0u;
        {
            std::uint32_t current_instance = 0;
            for (auto i = 0u; i < scene.GetMeshCount(); ++i)
            {
                res = rrBindAccelerationStructureMemory(context_, bottom_level_accel_[i], accel_buffer_.get(), offset);
                if (res != RR_STATUS_SUCCESS)
                {
                    throw std::runtime_error("Cannot bind acceleration structure memory");
                }
                res = rrBindAccelerationStructureBuildScratchMemory(context_, bottom_level_accel_[i], accel_build_buffer.get(), 0u);
                if (res != RR_STATUS_SUCCESS)
                {
                    throw std::runtime_error("Cannot bind acceleration structure build scratch memory");
                }
                RrInstanceBuildInfo& instance_info = build_infos[current_instance];
                instance_info.accelerationStructure = bottom_level_accel_[i];
                instance_info.instanceID = i;
                instance_info.instanceTransform = &instance_transforms[i * 12];
                // Unit transforms
                std::fill(instance_transforms.begin() + i * 12,
                          instance_transforms.begin() + (i + 1) * 12,
                          0.f);

                instance_transforms[i * 12] = 1;
                instance_transforms[i * 12 + 5] = 1;
                instance_transforms[i * 12 + 10] = 1;

                offset += meshes_mem_reqs[i].size;
            }
        }

        // Bind memory for the scene.
        res = rrBindAccelerationStructureMemory(context_, top_level_accel_, accel_buffer_.get(), offset);
        if (res != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot bind acceleration structure memory");
        }

        res = rrBindAccelerationStructureBuildScratchMemory(context_, top_level_accel_, accel_build_buffer.get(), 0);
        if (res != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot bind acceleration structure build scratch memory");
        }

        for (auto i = 0u; i < scene.GetMeshCount(); ++i)
        {
            RrAccelerationStructureBuildInfo build_info;

            build_info.buildOperation = RR_ACCELERATION_STRUCTURE_BUILD_OPERATION_BUILD;
            build_info.inputMemoryType = RR_ACCELERATION_STRUCTURE_INPUT_MEMORY_TYPE_CPU;
            build_info.numPrims = scene.meshes_[i].GetPrimCount();
            build_info.firstUpdateIndex = 0u;
            build_info.cpuMeshInfo.indexStride = scene.meshes_[i].IndexStride();
            build_info.cpuMeshInfo.pIndexData = scene.meshes_[i].Indices();
            build_info.cpuMeshInfo.pVertexData = scene.meshes_[i].Vertices();
            build_info.cpuMeshInfo.vertexStride = scene.meshes_[i].VertexStride();

            CommandBuffer build_accel_commands(vulkan_manager_->command_pool_, vulkan_manager_->device_);
            build_accel_commands.Begin();
            status = rrCmdBuildAccelerationStructure(context_, bottom_level_accel_[i], &build_info, build_accel_commands);
            build_accel_commands.End();
            if (status != RR_STATUS_SUCCESS)
            {
                throw std::runtime_error("Cannot create acceleration structure");
            }
            if (build_accel_commands.SubmitWait(vulkan_manager_->queue_) != VK_SUCCESS)
            {
                throw std::runtime_error("Cannot execute creating acceleration structure");
            }
        }

        // Build top level structure.
        RrAccelerationStructureBuildInfo build_info;
        build_info.buildOperation = RR_ACCELERATION_STRUCTURE_BUILD_OPERATION_BUILD;
        build_info.inputMemoryType = RR_ACCELERATION_STRUCTURE_INPUT_MEMORY_TYPE_CPU;
        build_info.firstUpdateIndex = 0u;
        build_info.numPrims = scene.GetMeshCount();

        // Build the acceleration structure
        CommandBuffer build_accel_commands(vulkan_manager_->command_pool_, vulkan_manager_->device_);
        build_info.cpuSceneInfo.pInstanceBuildInfo = build_infos.data();

        build_accel_commands.Begin();
        status = rrCmdBuildAccelerationStructure(context_, top_level_accel_, &build_info, build_accel_commands);
        build_accel_commands.End();
        if (status != RR_STATUS_SUCCESS)
        {
            throw std::runtime_error("Cannot create top-level acceleration structure");
        }
        if (build_accel_commands.SubmitWait(vulkan_manager_->queue_) != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot execute creating top-level acceleration structure");
        }

        std::cout << "CPUGPU acceleration structure has been built" << std::endl;

        return true;
    }
}