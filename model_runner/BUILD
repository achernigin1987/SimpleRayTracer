load(
    "//tensorflow:tensorflow.bzl",
    "tf_cc_binary",
)

LIB_SRCS = [
    "model_runner.h",
    "context.cpp",
    "context.h",
    "dtype.h",
    "image.cpp",
    "image.h",
    "model.cpp",
    "model.h",
    "utils.h",
]

tf_cc_binary(
    name = "libModelRunner.so",
    srcs = LIB_SRCS,
    copts = [
        "-std=c++1z",
        "-fvisibility=hidden",
        "-DRADEONPROML_BUILD",
    ],
    linkopts = [
        "-Wl,--exclude-libs,ALL",
        "-Wl,--strip-all",
    ],
    linkshared = True,
    linkstatic = True,
    deps = [
        "//tensorflow/cc:cc_ops",
        "//tensorflow/cc:client_session",
        "//tensorflow/core:tensorflow",
    ],
)

cc_import(
    name = "imported_libModelRunner",
    hdrs = ["model_runner.h"],
    shared_library = "libModelRunner.so",
)

cc_binary(
    name = "test_app",
    srcs = [
        "test_app.cpp",
    ],
    copts = [
        "-std=c++1z",
    ],
    includes = [
        "model_runner.h",
    ],
    linkopts = [
    ],
    deps = [
        ":imported_libModelRunner",
    ],
)
