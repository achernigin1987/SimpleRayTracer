#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace PathTracer
{
    constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();

    class Mesh;
    class Material;
    class Texture;


    class Scene
    {
    public:
        // Non-copyable
        Scene(Scene const&) = delete;
        Scene& operator =(Scene const&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        Scene();

        uint32_t GetMeshCount() const;
        uint32_t GetPrimCount() const;

        bool LoadFile(char const* filename);

        // Scene data
        std::vector<Mesh> meshes_;
        std::vector<Material> materials_;
        std::vector<Texture> textures_;

    private:
        static inline char const* GetFileExtension(char const* filename)
        {
            char const* fileExtension = strrchr(filename, '.');
            return (fileExtension ? fileExtension : filename);
        }

        bool ParseObj(char const* filename);
        bool ParseTex(char const* filename);

        std::string path_;
    };


    struct Texture
    {
        Texture(uint32_t width, uint32_t height, uint32_t channel_count)
            : upside_down_(false)
        {
            width_ = width;
            height_ = height;
            channel_count_ = channel_count;
            data_.resize(channel_count * width * height);
        }

        // Whether the texture needs flipping.
        bool upside_down_;
        uint32_t width_;
        uint32_t height_;
        uint32_t channel_count_;
        std::vector<uint8_t> data_;
    };

    class Material
    {
    public:
        Material()
            : albedo_(0.7f)
            , roughness_(1.0f)
            , metalness_(0.0f)
            , transparency_(0.0f)
            , reflection_ior_(1.5f)
            , refraction_ior_(1.0f)
            , albedo_map_(INVALID_ID)
            , roughness_map_(INVALID_ID)
            , metalness_map_(INVALID_ID)
            , normal_map_(INVALID_ID)
        {
        }

        // The albedo.
        glm::vec3 albedo_;
        // The roughness.
        float roughness_;
        // The metalness.
        float metalness_;
        // The transparency.
        float transparency_;
        // The reflection IOR.
        float reflection_ior_;
        // The refraction IOR.
        float refraction_ior_;

        // The albedo map.
        uint32_t albedo_map_;
        // The roughness map.
        uint32_t roughness_map_;
        // The metalness map.
        uint32_t metalness_map_;
        // The normal map.
        uint32_t normal_map_;
    };

    class Mesh
    {
    public:
        Mesh(std::string const& name, std::vector<float> const& vertices, std::vector<uint32_t> const& indices,
             uint32_t vertex_stride, uint32_t index_stride, uint32_t material_id)
            : name_(name)
            , vertices_(vertices)
            , vertex_stride_(vertex_stride)
            , indices_(indices)
            , index_stride_(index_stride)
            , material_id_(material_id)
        {
        }

        // Gets the number of primitives in the mesh
        inline uint32_t GetPrimCount() const
        {
            return GetIndexCount() / 3;
        }

        // Gets the number of indices in the mesh
        inline uint32_t GetIndexCount() const
        {
            return static_cast<uint32_t>(indices_.size()) / (index_stride_ / sizeof(uint32_t));
        }

        // Gets the number of vertices in the mesh
        inline uint32_t GetVertexCount() const
        {
            return static_cast<uint32_t>(vertices_.size()) / (vertex_stride_ / sizeof(float));
        }

        inline uint32_t GetVertexSize() const
        {
            return static_cast<uint32_t>(vertices_.size());
        }

        inline uint32_t GetIndexSize() const
        {
            return static_cast<uint32_t>(indices_.size());
        }

        float const* Vertices() const
        {
            return vertices_.data();
        }

        uint32_t const* Indices() const
        {
            return indices_.data();
        }

        uint32_t VertexStride() const
        {
            return vertex_stride_;
        }

        uint32_t IndexStride() const
        {
            return index_stride_;
        }

        uint32_t Material() const
        {
            return material_id_;
        }

    private:
        std::string name_;
        std::vector<float> vertices_;
        uint32_t vertex_stride_;
        std::vector<uint32_t> indices_;
        uint32_t index_stride_;
        uint32_t material_id_;
    };

}