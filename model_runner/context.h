#pragma once

#include "model_runner.h"

#include <sstream>


namespace ML {

class Image;
class Model;

class Context
{
public:
    static ml_context MakeHandle(Context* context);
    static Context* FromHandle(ml_context context);

    ml_image CreateImage(ml_image_info const* info);
    ml_model CreateModel(ml_model_params const* params);
    char* GetError(char* buffer, size_t buffer_size) const;

private:
    std::ostringstream m_error_cache;
};

} // namespace ML
