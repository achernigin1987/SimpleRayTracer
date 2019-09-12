#include "PathTracerImpl.h"
#include <iterator>

namespace PathTracer
{
    void PathTracerImpl::Init(Scene const& scene, RrAccelerationStructure top_level_structure, uint32_t num_rays, std::shared_ptr<VulkanManager> manager)
    {
        top_level_structure_ = top_level_structure;
        manager_ = manager;

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

        VkDeviceSize indices_size = VkDeviceSize(indices.size() * sizeof(uint32_t));
        VkDeviceSize vertices_size = VkDeviceSize(vertices.size() * sizeof(float));
        VkDeviceSize shapes_size = VkDeviceSize(shapes.size() * sizeof(Shape));
        VkDeviceSize color_size = VkDeviceSize(num_rays * sizeof(uint32_t));

        VkDeviceSize camera_rays_size = VkDeviceSize(num_rays * sizeof(RrRay));
        VkDeviceSize ao_rays_size = VkDeviceSize(num_rays * sizeof(RrRay));

        VkDeviceSize ao_count_size = VkDeviceSize(4u * sizeof(uint32_t));
        VkDeviceSize hits_size = VkDeviceSize(num_rays * sizeof(RrHit));
        VkDeviceSize random_size = VkDeviceSize(num_rays * sizeof(uint32_t));
        VkDeviceSize ao_size = VkDeviceSize(num_rays * sizeof(glm::uvec2));
        VkDeviceSize ao_id_size = VkDeviceSize(num_rays * sizeof(uint32_t));

        VkMemoryRequirements accel_trace_mem_reqs;
        rrGetAccelerationStructureTraceScratchMemoryRequirements(nullptr, top_level_structure, num_rays, &accel_trace_mem_reqs);
        VkDeviceSize scratch_trace_size = accel_trace_mem_reqs.size;
        VkDeviceSize params_size = VkDeviceSize(sizeof(Params));


    }
}