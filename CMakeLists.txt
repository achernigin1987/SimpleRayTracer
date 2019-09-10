cmake_minimum_required(VERSION 3.15)

project("SimpleRayTracer" VERSION 1.0.0.0)

if(WIN32)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
else(WIN32)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif(WIN32)

set(CMAKE_DEBUG_POSTFIX "-d")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

if(UNIX AND NOT APPLE)
    add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# TensorFlow-based model runner

# add_subdirectory(model_runner)
add_subdirectory(path_tracer)
