#include "context.h"

#include "image.h"
#include "model.h"
#include "utils.h"


namespace ML {

ml_context Context::MakeHandle(Context* context)
{
    return reinterpret_cast<ml_context>(context);
}

Context* Context::FromHandle(ml_context context)
{
    return reinterpret_cast<Context*>(context);
}

ml_image Context::CreateImage(ml_image_info const* info)
{
    m_error_cache.str("");

    try
    {
        return Image::MakeHandle(new Image(info));
    }
    catch (std::exception& e)
    {
        m_error_cache << e.what();
        return ML_INVALID_HANDLE;
    }
}


ml_model Context::CreateModel(ml_model_params const* params)
{
    m_error_cache.str("");

    try
    {
        return Model::MakeHandle(new Model(params));
    }
    catch (std::exception& e)
    {
        m_error_cache << e.what();
        return ML_INVALID_HANDLE;
    }
}

char* Context::GetError(char* buffer, size_t buffer_size) const
{
    return FillBuffer(buffer, buffer_size, m_error_cache.str());
}

} // namespace ML


ml_context mlCreateContext()
{
    try
    {
        return ML::Context::MakeHandle(new ML::Context);
    }
    catch (...)
    {
        return ML_INVALID_HANDLE;
    }
}

char* mlGetContextError(ml_context context, char* buffer, size_t buffer_size)
{
    if (ML::Context::FromHandle(context) == nullptr)
    {
        return ML::FillBuffer(buffer, buffer_size, "Bad context handle");
    }

    return ML::Context::FromHandle(context)->GetError(buffer, buffer_size);
}


ml_image mlCreateImage(ml_context context, ml_image_info const* info)
{
    if (ML::Context::FromHandle(context) == nullptr)
    {
        return ML_INVALID_HANDLE;
    }

    return ML::Context::FromHandle(context)->CreateImage(info);
}

ml_model mlCreateModel(ml_context context, ml_model_params const* params)
{
    if (ML::Context::FromHandle(context) == ML_INVALID_HANDLE)
    {
        return ML_INVALID_HANDLE;
    }

    return ML::Context::FromHandle(context)->CreateModel(params);
}

void mlReleaseContext(ml_context context)
{
    delete ML::Context::FromHandle(context);
}
