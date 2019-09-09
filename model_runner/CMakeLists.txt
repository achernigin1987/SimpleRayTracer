add_library(model_runner STATIC
    context.cpp
    context.h
    image.cpp
    image.h
    ml.h
    model.cpp
    model.h
    utils.h
)

target_include_directories(model_runner PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(model_runner PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/tensorflow_static.lib
)

add_executable(model_runner_app
    test_app.cpp
)

target_include_directories(model_runner_app PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(model_runner_app PRIVATE
    model_runner
)