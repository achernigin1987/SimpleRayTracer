#pragma once

#include "model_runner.h"

#include <algorithm>
#include <cstring>
#include <string>


namespace ML {

template<class Visitor>
bool ForEachDim(const Visitor& visitor)
{
    return visitor(&ml_image_info::width, "width")
        && visitor(&ml_image_info::height, "height")
        && visitor(&ml_image_info::channels, "channels");
}

inline char* FillBuffer(char* buffer, size_t buffer_size, std::string message)
{
    if (buffer != nullptr)
    {
        message.erase(std::min(message.size(), buffer_size) - 1);
        std::memcpy(buffer, message.c_str(), message.size() + 1);
    }
    return buffer;
};

} // namespace ML
