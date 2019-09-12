#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace PathTracer
{
    class Mesh;

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

    private:
        static inline char const* GetFileExtension(char const* filename)
        {
            char const* fileExtension = strrchr(filename, '.');
            return (fileExtension ? fileExtension : filename);
        }

        bool ParseObj(char const* filename);
        std::string path_;
    };

    class Mesh
    {
    public:
        Mesh(std::string const& name, std::vector<float> const& vertices, std::vector<uint32_t> const& indices, uint32_t vertex_stride, uint32_t index_stride)
            : name_(name)
            , vertices_(vertices)
            , vertex_stride_(vertex_stride)
            , indices_(indices)
            , index_stride_(index_stride)
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
            return static_cast<uint32_t>(vertices_.size()) / (vertex_stride_ / sizeof(uint32_t));
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

    private:
        std::string name_;
        std::vector<float> vertices_;
        uint32_t vertex_stride_;
        std::vector<uint32_t> indices_;
        uint32_t index_stride_;
    };
}