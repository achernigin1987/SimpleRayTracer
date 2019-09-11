#pragma once

#include "SceneController.h"
#include <fstream>
#include <tiny_obj_loader.h>

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

    // Constructor
    Scene::Scene()
    {
    }

    // Loads a file into the scene
    bool Scene::LoadFile(char const* filename)
    {
        assert(filename);

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
        const std::string relativePath(filename, file ? file + 1 : filename);

        // Parse the .obj file
        attrib_t attrib;
        std::string error;
        std::vector<shape_t> shapes;
        std::vector<material_t> materials;
        auto result = LoadObj(&attrib, &shapes, &materials, &error, filename, relativePath.empty() ? nullptr : relativePath.data());
        if (!error.empty())
        {
            std::cout << error << std::endl;
        }

        if (!result)
        {
            return false;
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
                                vertex[c + 9] = material->diffuse[c];
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
                meshes_.push_back(Mesh(shape.name, vertices, indices, sizeof(ObjVertex), sizeof(uint32_t)));
            }
            vertices.clear();
            indices.clear();
        }

        return true;
    }

}