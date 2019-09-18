#pragma once

#include "SceneController.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PKM
#include "../externals/soil2/src/SOIL2/stb_image.h"

namespace
{
    unsigned char* load_image(const char *filename, int *width, int *height, int *channels, int force_channels)
    {
        unsigned char *result = stbi_load(filename, width, height, channels, force_channels);
        if (result == NULL)
        {
            throw std::runtime_error(stbi_failure_reason());
        }

        return result;
    }
    void free_image_data(unsigned char *img_data)
    {
        if (img_data)
        {
            free((void*)img_data);
        }
    }

}

namespace PathTracer
{
    // Gets the number of meshes in the scene
    uint32_t Scene::GetMeshCount() const
    {
        return static_cast<uint32_t>(meshes_.size());
    }

    // Gets the number of primitives in the scene
    uint32_t Scene::GetPrimCount() const
    {
        uint32_t prim_count = 0;
        for (auto& mesh : meshes_)
        {
            prim_count += mesh.GetPrimCount();
        }
        return prim_count;
    }

    class ObjKey
    {
    public:
        inline ObjKey()
        {
        }

        inline bool operator <(const ObjKey &other) const
        {
            if (position_index_ != other.position_index_)
            {
                return (position_index_ < other.position_index_);
            }

            if (normal_index_ != other.normal_index_)
            {
                return (normal_index_ < other.normal_index_);
            }

            if (texcoords_index_ != other.texcoords_index_)
            {
                return (texcoords_index_ < other.texcoords_index_);
            }
            return false;
        }

        uint32_t    position_index_;
        uint32_t    normal_index_;
        uint32_t    texcoords_index_;
    };

    using ObjVertex = float[12];

    Scene::Scene() = default;

    // Loads a file into the scene
    bool Scene::LoadFile(char const* filename)
    {
        // Handle file type
        const std::string fileExtension = GetFileExtension(filename);
        if (fileExtension == ".obj")
        {
            return ParseObj(filename);
        }
        else
        {
            // Not supported
            return false;
        }
    }

    bool Scene::ParseObj(char const* filename)
    {
        using namespace tinyobj;

        // Compute relative path
        const char *file = std::max(strrchr(filename, '/'), strrchr(filename, '\\'));
        const std::string relative_path(filename, file ? file + 1 : filename);

        // Parse the .obj file
        attrib_t attrib;
        std::string error;
        std::vector<shape_t> shapes;
        std::vector<material_t> materials;
        auto result = LoadObj(&attrib, &shapes, &materials, &error, filename, relative_path.empty() ? nullptr : relative_path.data());
        if (!error.empty())
        {
            std::cout << error << std::endl;
        }

        if (!result)
        {
            return false;
        }
        std::map<std::string, uint32_t> found_materials;
        std::map<std::string, uint32_t> found_textures;

        for (auto& material : materials)
        {
            auto const default_reflection_ior = 3.0f;

            // Have we already created this material?
            auto material_id_it = found_materials.find(material.name);

            if (material_id_it != found_materials.end())
            {
                continue;   // re-use existing handle
            }

            // Create the material object
            materials_.emplace_back(Material());
            auto& m = materials_.back();

            // Create the texture objects
            if (!material.diffuse_texname.empty())
            {
                auto const texture = relative_path + material.diffuse_texname.c_str();
                auto it = found_textures.find(texture);
                if (found_textures.find(texture) != found_textures.end())
                {
                    m.albedo_map_ = it->second;
                }
                else
                {
                    if (!ParseTex(texture.c_str()))
                    {
                        continue;
                    }
                    // load texture file
                    found_textures.emplace(texture, (uint32_t)textures_.size() - 1);
                    m.albedo_map_ = (uint32_t)textures_.size() - 1;
                    auto& tex = textures_[m.albedo_map_];
                    for (auto i = 0u; i < tex.height_ * tex.width_; i++)
                    {
                        for (auto j = 0u; j < tex.channel_count_; ++j)
                        {
                            tex.data_[tex.channel_count_ * i + j] = std::uint8_t(glm::pow(tex.data_[tex.channel_count_ * i + j] / 255.0f, 2.2f) * 255.0f + 0.5f);
                        }
                    }
                    tex.upside_down_ = true;
                }
            }
            if (!material.roughness_texname.empty())
            {
                auto const texture = relative_path + material.roughness_texname.c_str();
                auto it = found_textures.find(texture);
                if (found_textures.find(texture) != found_textures.end())
                {
                    m.roughness_map_ = it->second;
                }
                else
                {
                    if (!ParseTex(texture.c_str()))
                    {
                        continue;
                    }
                    // load texture file
                    found_textures.emplace(texture, (uint32_t)textures_.size() - 1);
                    m.roughness_map_ = (uint32_t)textures_.size() - 1;
                    auto& tex = textures_[m.roughness_map_];
                    tex.upside_down_ = true;
                }
            }
            if (!material.metallic_texname.empty())
            {
                auto const texture = relative_path + material.metallic_texname.c_str();
                auto it = found_textures.find(texture);
                if (found_textures.find(texture) != found_textures.end())
                {
                    m.metalness_map_ = it->second;
                }
                else
                {
                    if (!ParseTex(texture.c_str()))
                    {
                        continue;
                    }
                    // load texture file
                    found_textures.emplace(texture, (uint32_t)textures_.size() - 1);
                    m.metalness_map_ = (uint32_t)textures_.size() - 1;
                    auto& tex = textures_[m.metalness_map_];
                    tex.upside_down_ = true;
                }
            }
            if (!material.normal_texname.empty())
            {
                auto const texture = relative_path + material.normal_texname.c_str();
                auto it = found_textures.find(texture);
                if (found_textures.find(texture) != found_textures.end())
                {
                    m.normal_map_ = it->second;
                }
                else
                {
                    if (!ParseTex(texture.c_str()))
                    {
                        continue;
                    }
                    // load texture file
                    found_textures.emplace(texture, (uint32_t)textures_.size() - 1);
                    m.normal_map_ = (uint32_t)textures_.size() - 1;
                    auto& tex = textures_[m.normal_map_];
                    tex.upside_down_ = true;
                }
            }

            // Populate base properties
            m.albedo_ = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
            m.roughness_ = 1.0f;
            m.metalness_ = 0.0f;
            m.transparency_ = 1.0f - material.dissolve;
            m.reflection_ior_ = 0.0f;
            m.refraction_ior_ = 0.0f;

            // Populate reflection properties
            auto const s = glm::vec3(material.specular[0], material.specular[1], material.specular[2]);
            auto const metalness = dot(s, s) / 3.0f;
            if (metalness > 0.0f)
            {
                m.reflection_ior_ = default_reflection_ior;
                m.roughness_ = material.roughness;
                m.metalness_ = metalness;
            }

            // Populate refraction properties
            if (material.illum == 4 ||
                material.illum == 6 ||
                material.illum == 7 ||
                material.illum == 9)
            {
                m.refraction_ior_ = material.ior;
            }

            // Populate transparency properties
            if (m.transparency_ > 0.0f)
            {
                m.reflection_ior_ = default_reflection_ior;
                m.roughness_ = 0.01f;
                m.metalness_ = 1.0f;
            }
        }

        // Create the scene data
        for (auto &shape : shapes)
        {
            // Gather the vertices
            ObjKey objKey;
            std::vector<float> vertices;
            std::vector<uint32_t> indices;
            std::map<ObjKey, uint32_t> objMap;

            uint32_t i = 0, material_id = 0;
            uint32_t mat_id = INVALID_ID;
            for (auto face = shape.mesh.num_face_vertices.begin(); face != shape.mesh.num_face_vertices.end(); i += *face, ++material_id, ++face)
            {
                // We only support triangle primitives
                if (*face != 3)
                {
                    continue;
                }
                // Load the material information
                auto material_idx = (!shape.mesh.material_ids.empty() ? shape.mesh.material_ids[material_id] : 0xffffffffu);
                auto material = (material_idx != 0xffffffffu ? &materials[material_idx] : nullptr);

                if (material == nullptr)
                {
                    continue;
                }
                mat_id = found_materials.find(material->name)->second;

                for (auto v = 0u; v < 3u; ++v)
                {
                    // Construct the lookup key
                    objKey.position_index_ = shape.mesh.indices[i + v].vertex_index;
                    objKey.normal_index_ = shape.mesh.indices[i + v].normal_index;
                    objKey.texcoords_index_ = shape.mesh.indices[i + v].texcoord_index;

                    // Look up the vertex map
                    uint32_t index;
                    auto it = objMap.find(objKey);
                    if (it != objMap.end())
                    {
                        index = (*it).second;
                    }
                    else
                    {
                        // Push the vertex into memory
                        ObjVertex vertex = {};
                        for (auto p = 0u; p < 3u; ++p)
                        {
                            vertex[p] = attrib.vertices[3 * objKey.position_index_ + p];
                        }
                        if (objKey.normal_index_ != 0xffffffffu)
                        {
                            for (auto n = 0u; n < 3u; ++n)
                            {
                                vertex[n + 3] = attrib.normals[3 * objKey.normal_index_ + n];
                            }
                        }
                        if (objKey.texcoords_index_ != 0xffffffffu)
                        {
                            for (auto t = 0u; t < 2u; ++t)
                            {
                                vertex[t + 6] = attrib.texcoords[2 * objKey.texcoords_index_ + t];
                            }
                        }
                        if (material != nullptr)
                        {
                            for (auto c = 0u; c < 3u; ++c)
                            {
                                vertex[c + 9] = glm::pow(material->diffuse[c], 2.2f);
                            }
                        }
                        for (auto value : vertex)
                        {
                            vertices.push_back(value);
                        }
                        index = static_cast<uint32_t>(vertices.size() / (sizeof(vertex) / sizeof(*vertex))) - 1;
                        objMap[objKey] = index;
                    }

                    // Push the index into memory
                    indices.push_back(index);
                }
            }
            if (!vertices.empty() && !indices.empty())
            {
                // Create the mesh object
                meshes_.push_back(Mesh(shape.name, vertices, indices, sizeof(ObjVertex), sizeof(uint32_t), mat_id));
            }
            vertices.clear();
            indices.clear();
        }

        return true;
    }

    bool Scene::ParseTex(char const* filename)
    {
        std::int32_t width, height, channel_count;
        try
        {
            auto* data = load_image(filename, &width, &height, &channel_count, 0);

            auto texture = Texture(width, height, channel_count);
            for (auto i = 0u; i < texture.width_ * texture.height_; i++)
            {
                for (auto j = 0u; j < texture.channel_count_; ++j)
                {
                    texture.data_[i * texture.channel_count_ + j] = data[i * texture.channel_count_ + j];
                }
            }

            textures_.emplace_back(texture);

            free_image_data(data);

        }
        catch (std::exception& e)
        {
            std::cout << "Failed to load " << filename << " due to " << e.what();
            return false;
        }

        return true;
    }


}