#define INVALID_ADDR 0xFFFFFFFFu

struct Hit
{
    uint shape_id;
    uint prim_id;
    vec2 uv;
};

struct Ray
{
    vec3 direction;
    float time;
    vec3 origin;
    float max_t;
};

struct Sampler
{
    uint index;
    uint scramble;
    uint dimension;
};

struct Shape
{
    uint count;
    uint first_index;
    uint base_vertex;
    uint padding;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoords;
    vec3 color;
};

struct Material
{
    vec3 albedo;
    float roughness;
    float metalness;
    float transparency;
    float reflection_ior;
    float refraction_ior;

    uint albedo_map;
    uint roughness_map;
    uint metalness_map;
    uint normal_map;
};