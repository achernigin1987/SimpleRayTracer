#include "image.h"

#include "utils.h"

#include <stdexcept>
#include <iostream>


namespace ML {

Image::Image(ml_image_info const* info)
{
    if (info == nullptr)
    {
        throw std::runtime_error("Bad image information argument");
    }

    if (info->dtype != ML_FLOAT32)
    {
        throw std::runtime_error("Unsupported image data type: " + std::to_string(info->dtype));
    }

    auto validate_dim = [info](auto dim, char const* name)
    {
        if (info->*dim != 0)
        {
            return true;
        }
        throw std::runtime_error(std::string("Unspecified image ") + name + " dimension");
    };

    ForEachDim(validate_dim);

    m_info = *info;
    m_data.resize(m_info.width * m_info.height * m_info.channels * sizeof(float));
}

ml_status Image::GetInfo(ml_image_info* info) const
{
    if (info == nullptr)
    {
        return ML_FAIL;
    }

    *info = m_info;
    return ML_OK;
}

void* Image::Map(size_t* size)
{
    if (size != nullptr)
    {
        *size = m_data.size();
    }

    return m_data.data();
}

ml_status Image::Unmap(void* data)
{
    if (data != m_data.data())
    {
        return ML_FAIL;
    }

    return ML_OK;
}

} // namespace ML


ml_status mlGetImageInfo(ml_image image, ml_image_info* info)
{
    if (ML::Image::FromHandle(image) == nullptr)
    {
        return ML_FAIL;
    }

    return ML::Image::FromHandle(image)->GetInfo(info);
}

void* mlMapImage(ml_image image, size_t* size)
{
    if (ML::Image::FromHandle(image) == nullptr)
    {
        return nullptr;
    }

    return ML::Image::FromHandle(image)->Map(size);
}

ml_status mlUnmapImage(ml_image image, void* data)
{
    if (ML::Image::FromHandle(image) == nullptr)
    {
        return ML_FAIL;
    }

    return ML::Image::FromHandle(image)->Unmap(data);
}

void mlReleaseImage(ml_image image)
{
    delete ML::Image::FromHandle(image);
}
