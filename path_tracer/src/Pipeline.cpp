#include "Pipeline.h"
#include <fstream>
#include <iostream>

namespace PathTracer
{
    // Constructor
    Binding::Binding()
        : buffer_(VK_NULL_HANDLE)
        , offset_(0u)
        , range_(0u)
    {}

    // Constructor
    Binding::Binding(VkBuffer const& buffer, VkDeviceSize offset, VkDeviceSize range)
        : buffer_(buffer)
        , offset_(offset)
        , range_(range)
    {}

    Binding::Binding(std::vector<VkDescriptorImageInfo> const& infos)
        : buffer_(VK_NULL_HANDLE)
        , offset_(0u)
        , range_(0u)
        , image_infos_(infos)
    {
    }


    // Constructor
    Pipeline::Pipeline(std::shared_ptr<VulkanManager> vk_manager)
        : pipeline_(VK_NULL_HANDLE)
        , pipeline_layout_(VK_NULL_HANDLE)
        , shader_module_(VK_NULL_HANDLE)
        , descriptor_set_layout_(VK_NULL_HANDLE)
        , vk_manager_(vk_manager)
    {}

    // Destructor
    Pipeline::~Pipeline()
    {
        VkResult result = Destroy();

        if (result != VK_SUCCESS)
        {
            std::cerr << "Cannot destroy pipeline structure" << std::endl;
        }
    }

    // Creates a pipeline object
    VkResult Pipeline::Create(char const* shader_file, DescriptorTypeInfo const* descriptor_types, uint32_t descriptor_type_count, uint32_t push_constants_size)
    {
        Destroy();
        VkResult result;
        uint32_t binding = 0;
        std::vector<char> shader_source = ReadFile(shader_file);
        VkDevice device = vk_manager_->device_;
        descriptor_set_layout_bindings_.resize(descriptor_type_count);

        // Initialize bindings
        for (auto& descriptor_set_layout_binding : descriptor_set_layout_bindings_)
        {
            descriptor_set_layout_binding = {};
            descriptor_set_layout_binding.binding = binding;
            descriptor_set_layout_binding.descriptorType = descriptor_types[binding].descriptor_type_;
            descriptor_set_layout_binding.descriptorCount = descriptor_types[binding].count_;
            descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            binding++;
        }

        // Create the descriptor set layout
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
        descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = descriptor_type_count;
        descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings_.data();
        result = vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout_);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create descriptor set layout");
        }

        VkPushConstantRange push_constant_range = {};
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = push_constants_size;

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout_;
        pipeline_layout_create_info.pushConstantRangeCount = (push_constants_size > 0 ? 1 : 0);
        pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
        result = vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout_);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create pipeline layout");
        }


        // Create the shader module
        VkShaderModuleCreateInfo shader_module_create_info = {};
        shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.codeSize = shader_source.size();
        shader_module_create_info.pCode = reinterpret_cast<const uint32_t *>(shader_source.data());
        result = vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_module_);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create shader module");
        }

        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info = {};
        pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_shader_stage_create_info.module = shader_module_;
        pipeline_shader_stage_create_info.pName = "main";

        // Create the pipeline object
        VkComputePipelineCreateInfo compute_pipeline_create_info = {};
        compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compute_pipeline_create_info.stage = pipeline_shader_stage_create_info;
        compute_pipeline_create_info.layout = pipeline_layout_;
        result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &pipeline_);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot create compute pipelines");
        }

        return result;
    }

    // Destroys the pipeline object
    VkResult Pipeline::Destroy()
    {
        VkDevice device = vk_manager_->device_;
        
        // Release our resources
        if (pipeline_)
        {
            vkDestroyPipeline(device, pipeline_, nullptr);
        }
        if (pipeline_layout_)
        {
            vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
        }
        if (shader_module_)
        {
            vkDestroyShaderModule(device, shader_module_, nullptr);
        }
        if (descriptor_set_layout_)
        {
            vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
        }
        if (descriptor_sets_.size() > 0)
        {
            vkFreeDescriptorSets(device, vk_manager_->descriptor_pool_, static_cast<uint32_t>(descriptor_sets_.size()), descriptor_sets_.data());
            descriptor_sets_.clear();
        }
        // Reset the members
        pipeline_ = VK_NULL_HANDLE;
        pipeline_layout_ = VK_NULL_HANDLE;
        shader_module_ = VK_NULL_HANDLE;
        descriptor_set_layout_ = VK_NULL_HANDLE;

        return VK_SUCCESS;
    }

    // Binds the buffers to a descriptor set
    VkDescriptorSet Pipeline::Bind(Binding const* bindings, uint32_t binding_count)
    {
        VkResult result;
        VkDescriptorSet descriptor_set;
        VkDevice device = vk_manager_->device_;

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = vk_manager_->descriptor_pool_;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout_;
        result = vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, &descriptor_set);
        descriptor_sets_.push_back(descriptor_set);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Cannot allocate descriptor sets");
        }

        // Write to the descriptor set
        std::vector<VkWriteDescriptorSet> write_descriptor_sets(binding_count);
        std::vector<VkDescriptorBufferInfo> descriptor_buffer_infos(binding_count);

        for (uint32_t binding = 0; binding < binding_count; ++binding)
        {
            auto& write_descriptor_set = write_descriptor_sets[binding];
            auto& descriptor_buffer_info = descriptor_buffer_infos[binding];

            descriptor_buffer_info = {};
            descriptor_buffer_info.buffer = bindings[binding].buffer_;
            descriptor_buffer_info.offset = bindings[binding].offset_;
            descriptor_buffer_info.range = bindings[binding].range_;

            write_descriptor_set = {};
            write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_descriptor_set.dstSet = descriptor_set;
            write_descriptor_set.dstBinding = descriptor_set_layout_bindings_[binding].binding;
            write_descriptor_set.descriptorCount = descriptor_set_layout_bindings_[binding].descriptorCount;
            write_descriptor_set.descriptorType = descriptor_set_layout_bindings_[binding].descriptorType;
            if (bindings[binding].image_infos_.empty())
            {
                write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
            }
            else
            {
                write_descriptor_set.pImageInfo = bindings[binding].image_infos_.data();
            }
        }
        vkUpdateDescriptorSets(device, binding_count, write_descriptor_sets.data(), 0, nullptr);

        return descriptor_set;
    }

    // Reads the content of the file
    std::vector<char> Pipeline::ReadFile(const char* filename)
    {
        if (!filename)
        {
            throw std::runtime_error("Filename was not set");
        }
        std::vector<char> fileContent;
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Error: Unable to open file `" << filename << "'" << std::endl;
        }
        else
        {
            const size_t fileSize = static_cast<size_t>(file.tellg());
            fileContent.resize(fileSize);
            file.seekg(0);
            file.read(fileContent.data(), fileSize);
            file.close();
        }
        return fileContent;
    }
}