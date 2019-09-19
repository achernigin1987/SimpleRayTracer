/**********************************************************************
Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#pragma once

#include "VulkanManager.h"
#include <vector>

namespace PathTracer
{

    // Forward declarations
    class BufferBase;

    // Specifies an individual buffer binding
    class Binding
    {
    public:
        Binding();
        Binding(VkBuffer const& buffer, VkDeviceSize offset = 0u, VkDeviceSize range = VK_WHOLE_SIZE);
        Binding(std::vector<VkDescriptorImageInfo> const&);
        // The buffer to be bound
        VkBuffer buffer_;
        // The offset for the buffer range
        VkDeviceSize offset_;
        // The size of the buffer range
        VkDeviceSize range_;
        // image info
        std::vector<VkDescriptorImageInfo> image_infos_;
    };

    // A helper for dealing with Vulkan pipeline objects
    class Pipeline
    {
        // Non-copyable
        Pipeline(Pipeline const&) = delete;
        Pipeline& operator =(Pipeline const&) = delete;

    public:
        Pipeline(std::shared_ptr<VulkanManager> vk_manager);
        ~Pipeline();

        template<uint32_t COUNT>
        VkResult Create(char const* shader_file, VkDescriptorType const (&descriptor_types)[COUNT], uint32_t push_constants_size = 0);
        VkResult Create(char const* shader_file, VkDescriptorType const* descriptor_types, uint32_t descriptor_type_count, uint32_t push_constants_size = 0);
        VkResult Destroy();

        template<uint32_t COUNT>
        VkDescriptorSet Bind(Binding const (&bindings)[COUNT]);
        VkDescriptorSet Bind(Binding const* bindings, uint32_t binding_count);

        inline operator VkPipeline() const;
        inline VkPipeline GetPipeline() const;
        inline VkPipelineLayout GetPipelineLayout() const;

    private:
        static std::vector<char> ReadFile(char const* filename);

        // The pipeline object
        VkPipeline pipeline_;
        // The pipeline layout object
        VkPipelineLayout pipeline_layout_;
        // The shader module
        VkShaderModule shader_module_;
        // The descriptor set layout
        VkDescriptorSetLayout descriptor_set_layout_;
        // The descriptor set layout bindings
        std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings_;
        // The descriptor sets pools
        std::vector<VkDescriptorSet> descriptor_sets_;

        std::shared_ptr<VulkanManager> vk_manager_;
    };

    // Creates a pipeline object
    template<uint32_t COUNT>
    VkResult Pipeline::Create(char const* shader_file, VkDescriptorType const (&descriptor_types)[COUNT], uint32_t push_constants_size)
    {
        return Create(shader_file, descriptor_types, COUNT, push_constants_size);
    }

    // Binds the buffers to a descriptor set
    template<uint32_t COUNT>
    VkDescriptorSet Pipeline::Bind(Binding const (&bindings)[COUNT])
    {
        return Bind(bindings, COUNT);
    }

    // Gets the pipeline object
    Pipeline::operator VkPipeline() const
    {
        return pipeline_;
    }

    // Gets the pipeline object
    VkPipeline Pipeline::GetPipeline() const
    {
        return pipeline_;
    }

    // Gets the pipeline layout
    VkPipelineLayout Pipeline::GetPipelineLayout() const
    {
        return pipeline_layout_;
    }
}