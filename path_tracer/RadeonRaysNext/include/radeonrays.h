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

#include <vulkan/vulkan.h>
#include <stdint.h>

#define RR_MAKE_VERSION(a,b,c) (((a) << 22) | ((b) << 12) | (c))

#define RR_API_VERSION RR_MAKE_VERSION(1, 0, 0)

#define RR_STATIC_LIBRARY

#ifndef RR_STATIC_LIBRARY
#ifdef WIN32
#ifdef EXPORT_API
#define RR_API __declspec(dllexport)
#else
#define RR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#ifdef EXPORT_API
#define RR_API __attribute__((visibility ("default")))
#else
#define RR_API
#endif
#endif
#else
#define RR_API
#endif

/**
    Return codes for API functions.
*/
enum RrStatus
{
    RR_STATUS_SUCCESS = 0,
    RR_STATUS_INVALID_VALUE = -1,
    RR_STATUS_NOT_IMPLEMENTED = -2,
    RR_STATUS_OUT_OF_SYSTEM_MEMORY = -3,
    RR_STATUS_OUT_OF_VIDEO_MEMORY = -4,
    RR_STATUS_INTERNAL_ERROR = -5,
    RR_STATUS_DEVICE_NOT_SUPPORTED = -6,
    RR_STATUS_INCOMPATIBLE_API = -7
};

/**
    Marker for invalid IDs (on ray misses, etc).
*/
enum
{
    RR_INVALID_ID = 0xffffffffu
};

/**
    Query for TraceRays function:
        - Intersect means find closest hit and terminate.
        - Occluded means find any hit and terminate.
*/
enum RrQueryType
{
    RR_QUERY_TYPE_INTERSECT = 1,
    RR_QUERY_TYPE_OCCLUDED = 2
};

/**
    Output type for TraceRays function:
    - Full hit means write full RrHit structure for each ray (16 bytes).
    - Instance ID only means write only instance ID for each ray (4 bytes).
    - Compressed bit means write a single bit per ray, only supported for 
      occlusion query type.
*/
enum RrOutputType
{
    RR_OUTPUT_TYPE_FULL_HIT = 1,
    RR_OUTPUT_TYPE_INSTANCE_ID_ONLY = 2,
    RR_OUTPUT_TYPE_COMPRESSED_BIT = 3
};

/**
    Trace flag for TraceRays function:
    - Update max T means update of maxT value in RrRay structure using intersection T value

*/
enum RrTraceFlagBits
{
    RR_TRACE_FLAG_UPDATE_MAX_T = (1 << 0),
    RR_TRACE_FLAG_UPDATE_TRUE_HITS_ONLY = (1 << 1),
};

/**
    Acceleration structure type.
*/
enum RrAccelerationStructureType
{
    RR_ACCELERATION_STRUCTURE_TYPE_MESH = 1,
    RR_ACCELERATION_STRUCTURE_TYPE_SCENE = 2,
    RR_ACCELERATION_STRUCTURE_TYPE_FLATTENED_SCENE = 3,
};

/**
    Type of memory acceleration structure takes its input data from.
*/
enum RrAccelerationStructureInputMemoryType
{
    RR_ACCELERATION_STRUCTURE_INPUT_MEMORY_TYPE_CPU = 1,
    RR_ACCELERATION_STRUCTURE_INPUT_MEMORY_TYPE_GPU = 2
};

/**
    Various acceleration structure flags.
*/
enum RrAccelerationStructureCreateFlagBits
{
    RR_ACCELERATION_STRUCTURE_FLAGS_DYNAMIC = (1 << 0),
    RR_ACCELERATION_STRUCTURE_FLAGS_PREFER_FAST_BUILD = (1 << 1),
    RR_ACCELERATION_STRUCTURE_FLAGS_ALLOW_GPU_INPUTS = (1 << 2),
    RR_ACCELERATION_STRUCTURE_FLAGS_USE_RTX = (1 << 3),
    RR_ACCELERATION_STRUCTURE_FLAGS_COMPRESSED = (1 << 4),
    RR_ACCELERATION_STRUCTURE_FLAGS_SPARSE = (1 << 5)
};

/**
    Build operation type.
*/
enum RrAccelerationStructureBuildOperation
{
    RR_ACCELERATION_STRUCTURE_BUILD_OPERATION_BUILD = 1,
    RR_ACCELERATION_STRUCTURE_BUILD_OPERATION_UPDATE = 2
};

VK_DEFINE_HANDLE(RrContext);
VK_DEFINE_HANDLE(RrAccelerationStructure);
typedef uint32_t   RrAccelerationStructureCreateFlags;
typedef uint32_t   RrTraceFlags;

/**
    Ray structure.
*/
struct RrRay
{
    // Ray direction (X, Y, Z).
    float direction[3];
    // Ray minimum distance.
    float minT;
    // Ray origin (X, Y, Z).
    float origin[3];
    // Ray maximum distance.
    float maxT;
};

/**
    Hit structure.
*/
struct RrHit
{
    // ID of the shape (RR_INVALID_ID in case of a miss).
    uint32_t instanceID;
    // ID of a primitive (undefined in case of a miss).
    uint32_t primitiveID;
    // Barycentric coordinates of a hit (undefined in case of a miss).
    float uv[2];
};

/**
    Context descriptor for RR context.
*/

struct RrApplicationInfo
{
    char const*  pApplicationName;
    uint32_t     applicationVersion;
    char const*  pEngineName;
    uint32_t     engineVersion;
    uint32_t     apiVersion;
    uint32_t     cachedDescriptorsNumber;
};

struct RrContextCreateInfo
{
    RrApplicationInfo const* applicationInfo;
};

/**
    Instance descriptor for Scene acceleration structure build
*/
struct RrInstanceBuildInfo
{
    // Instance ID goes into RrHit::instID if corresponding instance is hit.
    uint32_t instanceID;
    // Instance transform (12 floats, row major).
    float const* instanceTransform;
    // Reference to an acceleration structure, 
    // only MESH accelerating structure is supported here.
    RrAccelerationStructure accelerationStructure;
};

/**
    Acceleration structure specification.
*/
struct RrAccelerationStructureCreateInfo
{
    // Type of the acceleration structure (mesh, scene, etc).
    RrAccelerationStructureType type;
    // Hints for the library.
    RrAccelerationStructureCreateFlags flags;
    // Maximum number of primitives in this structure.
    uint32_t maxPrims;
};

struct RrInstanceBuildInfoGPU
{
    // Acceleration structure offset relative to first acceleration
    // structure residing in VkDeviceMemory in 64byte units
    uint32_t accelerationStructureOffset;
    //  ID of the instance
    uint32_t instanceID;
    // Index of the transform in transform buffer
    uint32_t transformOffset;
    // Padding of the structure
    uint32_t padding;
};

/**
    Acceleration structure build data.
*/
struct RrAccelerationStructureBuildInfo
{
    // Build operation
    RrAccelerationStructureBuildOperation buildOperation;
    // Type of memory input data comes from.
    RrAccelerationStructureInputMemoryType inputMemoryType;
    // Number of primitives.
    uint32_t numPrims;
    // Update range starting index (should be 0 if buildOperation == BUILD).
    uint32_t firstUpdateIndex;
    // Parameter to set optimization steps, bigger value - less build performance - more trace performance.
    // Must be set along with absence of RR_ACCELERATION_STRUCTURE_FLAGS_PREFER_FAST_BUILD flag
    uint32_t optimizationSteps;

    // Data source.
    union
    {
        // If type == MESH and memoryType == CPU.
        struct
        {
            float const* pVertexData;
            uint32_t vertexStride;
            uint32_t const* pIndexData;
            uint32_t indexStride;
        } cpuMeshInfo;

        // If type == MESH and memoryType == GPU.
        struct
        {
            VkBuffer indexBuffer;
            VkBuffer vertexBuffer;
            uint32_t vertexStride;
            uint32_t indexStride;
            uint32_t indexSize; // Size of index in bytes, 2 or 4
            uint32_t vertexBufferOffset; // Vertices buffer offset for mesh of interest in bytes
            uint32_t indexBufferOffset; // Indices buffer offset for mesh of interest in bytes
        } gpuMeshInfo;

        // If type == SCENE and memory == CPU.
        struct
        {
            // The limitations is that all MESH acc structures,
            // referenced in pInstanceBuildInfo, should reside
            // in the same VkDeviceMemory segment to cope with
            // the lack of device pointer in Vulkan.
            RrInstanceBuildInfo* pInstanceBuildInfo;
        } cpuSceneInfo;

        // If type == SCENE and memory == GPU.
        struct
        {
            RrAccelerationStructure const* pAccelerationStructures;
            VkBuffer buildInfo;
            VkBuffer transforms;
        } gpuSceneInfo;

        // If type == FLATTENED_SCENE and memoryType == CPU.
        struct
        {
            uint32_t numMeshes;
            float const* const* ppVertexData;
            uint32_t const* pVertexStrides;
            uint32_t const* const* ppIndexData;
            uint32_t const* pIndexStrides;
            uint32_t const* pPrimCounts;
            uint32_t const* pIDs;
            float const* const* ppTransforms;
        } cpuFlattenedSceneInfo;

        // If type == FLATTENED_SCENE and memoryType == GPU
        struct
        {
            uint32_t numMeshes;
            VkBuffer vertexData;
            VkBuffer baseVertices;
            VkBuffer vertexStrides;
            VkBuffer indexData;
            VkBuffer firstIndices;
            VkBuffer indexStrides;
            VkBuffer primCounts;
            VkBuffer ids;
            VkBuffer transforms;
        } gpuFlattenedSceneInfo;
    };
};

// API functions
#ifdef __cplusplus
extern "C"
{
#endif
    /**
        Create an instance of RadeonRays API. All successive API calls require this context as a first
        parameter.

        \param device Vulkan device for API to operate on.
        \param physicalDevice Vulkan physical device.
        \param outContext API context
        \return RR_STATUS_SUCCESS or RR_STATUS_DEVICE_NOT_SUPPORTED
    */
    RR_API RrStatus rrCreateContext(VkDevice device, VkPhysicalDevice physicalDevice, RrContextCreateInfo const* info, RrContext* outContext);

    /**
        Release the instance of RadeonRays API.
    */
    RR_API RrStatus rrDestroyContext(RrContext context);

    /**
        Create an acceleration structure object. Note, that this function does 
        not perform any memory allocations or build processes. Memory should be 
        explicitly bound to an acceleration structure after the creation and build
        is only possible if enough memory is bound.

        \param context RadeonRays context.
        \param info Acceleration structure specification.
        \param outAccStructure Acceleration structure object.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus rrCreateAccelerationStructure(RrContext context,
                                                  RrAccelerationStructureCreateInfo const* info,
                                                  RrAccelerationStructure* outAccStructure);

    /**
        Delete an acceleration structure

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus rrDestroyAccelerationStructure(RrContext context, RrAccelerationStructure accStructure);

    /**
        Get memory requirements to store an acceleration structure. Memory requirements are based on
        an acceleration structure type and maxPrims specified at the creation stage.

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param outMemoryRequirements Memory requirements.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus rrGetAccelerationStructureMemoryRequirements(RrContext context,
                                                                 RrAccelerationStructure accStructure,
                                                                 VkMemoryRequirements* outMemoryRequirements);

    /**
        Get memory requirements for scratch space used for an acceleration structure build. 
        Memory requirements are based on an acceleration structure type and maxPrims 
        specified at the creation stage.

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param outMemoryRequirements Memory requirements.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus
    rrGetAccelerationStructureBuildScratchMemoryRequirements(RrContext context,
                                                             RrAccelerationStructure accStructure,
                                                             VkMemoryRequirements* outMemoryRequirements);

    /**
        Get memory requirements for scratch space used for an acceleration structure build. 
        Memory requirements are based on an acceleration structure type and maxPrims 
        specified at the creation stage.

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param outMemoryRequirements Memory requirements.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus
    rrGetAccelerationStructureTraceScratchMemoryRequirements(RrContext context,
                                                             RrAccelerationStructure accStructure,
                                                             uint32_t numRays,
                                                             VkMemoryRequirements* outMemoryRequirements);

    /**
        Bind memory to store an acceleration structure. Memory should have VK_MEMORY_TYPE_DEVICE_LOCAL_BIT property and
        the size should be >= the size returned from rrAccelerationStructureGetMemoryRequirements.

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param memory Memory region.
        \param offset Offset withing the region.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus rrBindAccelerationStructureMemory(RrContext context,
                                                      RrAccelerationStructure accStructure,
                                                      VkDeviceMemory memory,
                                                      VkDeviceSize offset);

    /**
        Bind scratch memory space being used by build operations.
        Memory should have VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT properties.

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param memory Memory region.
        \param offset Offset withing the region.
        \return RR_STATUS_SUCCESS or RR_STATUS_INVALID_VALUE.
    */
    RR_API RrStatus rrBindAccelerationStructureBuildScratchMemory(RrContext context,
                                                                  RrAccelerationStructure accStructure,
                                                                  VkDeviceMemory memory,
                                                                  VkDeviceSize offset);

    /**
        Do necessary CPU work and record acceleration structure build commands into the 
        specified command buffer. If SCENE acceleration structure is being used it is required,
        that all the MESH acceleration structures reside in the same VkDeviceMemory (to overcome
        Vulkan's lack of device pointer).

        \param context RadeonRays context.
        \param accStructure Acceleration structure object.
        \param info Build parameters.
        \param scratchMemory Scratch space for the build.
        \param scratchMemoryOffset Scratch space offset.
        \param inoutCommandBuffer Command buffer to record to.
        \return RR_STATUS_SUCCESS or RR_STATUS_OUT_OF_SYSTEM_MEMORY.
    */
    RR_API RrStatus rrCmdBuildAccelerationStructure(RrContext context,
                                                    RrAccelerationStructure accStructure,
                                                    RrAccelerationStructureBuildInfo const* info,
                                                    VkCommandBuffer inoutCommandBuffer);

    /**
        Record requested trace operation into the specified command buffer.

        \param context RadeonRays context.
        \param accStructure Acceleration structure.
        \param queryType The type of the query to record.
        \param outputType Type of the information to output for each ray.
        \param traceFlags Additional parameters of the trace.
        \param numRays Number of ray to trace from rayBuffer.
        \param traceBuffers 
                            Ray buffer
                            Hit buffer
        \param scratchBuffer
                            Scratch space
        \param inoutCommandBuffer Command buffer to record to.
        \return RR_STATUS_SUCCESS or RR_STATUS_OUT_OF_SYSTEM_MEMORY.
    */
    RR_API RrStatus rrCmdTraceRays(RrContext context,
                                   RrAccelerationStructure accStructure,
                                   RrQueryType queryType,
                                   RrOutputType outputType,
                                   RrTraceFlags traceFlags,
                                   uint32_t numRays,
                                   VkBuffer rays,
                                   VkBuffer hits,
                                   VkBuffer scratch,
                                   VkCommandBuffer inoutCommandBuffer);

    /**
        Record requested trace operation into the specified command buffer taking 
        ray count from device local memory.

        \param context RadeonRays context.
        \param accStructure Acceleration structure.
        \param queryType The type of the queury to record.
        \param outputType Type of the information to output for each ray.
        \param traceFlags Additional parameters of the trace.
        \param rayCountBuffer Buffer containing indirect dispatch params.
        \param rayCountOffset Offset in ray count buffer.
        \param traceBuffers 
                            Ray buffer.
                            Hit buffer.
                            Ray count.
        \param scratchBuffer
                            Scratch space.
        \param inoutCommandBuffer Command buffer to record to.
        \return RR_STATUS_SUCCESS or RR_STATUS_OUT_OF_SYSTEM_MEMORY.
    */
    RR_API RrStatus rrCmdTraceRaysIndirect(RrContext context,
                                           RrAccelerationStructure accStructure,
                                           RrQueryType queryType,
                                           RrOutputType outputType,
                                           RrTraceFlags traceFlags,
                                           VkBuffer rays,
                                           VkBuffer hits,
                                           VkBuffer rayCount,
                                           VkBuffer scratch,
                                           VkCommandBuffer inoutCommandBuffer);

    /**
        Set path to RadeonRays SPIR-V kernels (can be relative).

        \param context RadeonRays context.
        \param path Absolute or relative kernel path.
        \return RR_STATUS_SUCCESS or invalid value if path == nullptr.
    */
    RR_API RrStatus rrSetKernelsPath(RrContext context, char const* path);

    /**
    Reset cached descriptor sets
    \return RR_STATUS_SUCCESS or RR_STATUS_INTERNAL_ERROR
    */
    RR_API RrStatus rrResetCachedDescriptorSets(RrContext context);
#ifdef __cplusplus
}
#endif
