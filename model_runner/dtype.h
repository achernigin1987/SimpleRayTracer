#pragma once

#include "model_runner.h"

#include "tensorflow/core/framework/types.pb.h"


namespace ML {

inline tensorflow::DataType DataTypeToTF(ml_data_type type)
{
    switch (type)
    {
        case ML_FLOAT32:
            return tensorflow::DT_FLOAT;

        case ML_FLOAT16:
            return tensorflow::DT_HALF;

        default:
            throw std::runtime_error("Unsupported image data type: " + std::to_string(type));
    }
}

inline ml_data_type DataTypeFromTF(tensorflow::DataType type)
{
    switch (type)
    {
        case tensorflow::DT_FLOAT:
            return ML_FLOAT32;

        case tensorflow::DT_HALF:
            return ML_FLOAT16;

        default:
            throw std::runtime_error("Unsupported image data type: " + std::to_string(type));
    }
}

inline size_t DataTypeSize(ml_data_type type)
{
    switch (type)
    {
        case ML_FLOAT32:
            return 4;

        case ML_FLOAT16:
            return 2;

        default:
            throw std::runtime_error("Unsupported image data type: " + std::to_string(type));
    }
}

} // namespace ML
