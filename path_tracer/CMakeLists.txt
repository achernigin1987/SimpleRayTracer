find_package(Vulkan)

SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
SET(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# GLFW 3.2.1
add_subdirectory(externals/glfw-3.2.1)
# GLM 0.9.8.5
add_subdirectory(externals/glm-0.9.8.5)
#tinyobjloader 1.1.0
add_subdirectory(externals/tinyobjloader-1.1.0)

#--------------------------------------------------------------------
# Build example
#--------------------------------------------------------------------
function(BuildApp APP_NAME)
    # Get source files
    file(GLOB HEADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
    file(GLOB SHADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.geom
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert)
    file(GLOB SOURCE_FILES  
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

    # Add executable
    add_executable(${APP_NAME} ${SOURCE_FILES})

    target_sources(${APP_NAME} PRIVATE
        ${HEADER_FILES}
        ${SHADER_FILES})

    
    include(KernelUtils)

    KernelUtils_build_kernels(
        SOURCE_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        SOURCES
        ${SHADER_FILES}
        PARAMETERS --target-env vulkan1.0
    )

    KernelUtils_add_build_kernel_target(${APP_NAME})

    set_target_properties(${APP_NAME} PROPERTIES
        FOLDER "Examples"
        DEBUG_POSTFIX D
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    source_group("Shaders" FILES ${SHADER_FILES})
    
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/RadeonRaysNext/include)

    target_link_libraries(${APP_NAME} PUBLIC
        glfw
        glm
        ${CMAKE_CURRENT_SOURCE_DIR}/RadeonRaysNext/lib/RadeonRaysNext.lib
        tinyobjloader
        Vulkan::Vulkan)
endfunction(BuildApp)

#--------------------------------------------------------------------
# Build
#--------------------------------------------------------------------

BuildApp(PathTracer)
