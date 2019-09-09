#pragma once

#include "ml.h"

#include <vector>


namespace ML {

class Image
{
public:
    static ml_image MakeHandle(Image* image)
    {
        return reinterpret_cast<ml_image>(image);
    }

    static Image* FromHandle(ml_image image)
    {
        return reinterpret_cast<Image*>(image);
    }

    explicit Image(ml_image_info const* info);

    ml_status GetInfo(ml_image_info* info) const;
    void* Map(size_t* size);
    ml_status Unmap(void* data);

private:
    ml_image_info m_info;
    std::vector<char> m_data;
};

} // namespace ML
