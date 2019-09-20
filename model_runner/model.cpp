#include "model.h"

#include "dtype.h"
#include "image.h"
#include "utils.h"

#include <cstring>


#define PRINT_GRAPH_INFO 0


namespace tf = tensorflow;

namespace {

tf::SessionOptions CreateSessionOptions(const ml_model_params& params)
{
    tf::SessionOptions options;
    return options;
}

void FillImageInfo(const tf::NodeDef& node, ml_image_info& info)
{
    auto dtype_iter = node.attr().find("dtype");
    if (dtype_iter == node.attr().end())
    {
        dtype_iter = node.attr().find("T");
    }
    if (dtype_iter == node.attr().end())
    {
        throw std::runtime_error("Unable to detect node dtype: " + node.name());
    }

    info.dtype = ML::DataTypeFromTF(dtype_iter->second.type());

    auto& shape = node.attr().at("_output_shapes").list().shape(0);
    int dims = shape.dim_size();
    auto GetDim = [&shape, dims](int index)
    {
        int value = shape.dim(dims + index).size();
        return value != -1 ? value : 0;
    };

    info.height = GetDim(-3);
    info.width = GetDim(-2);
    info.channels = GetDim(-1);
}

void FillImageInfo(const tf::Tensor& tensor, ml_image_info& info)
{
    int dims = tensor.dims();
    info.height = tensor.dim_size(dims - 3);
    info.width = tensor.dim_size(dims - 2);
    info.channels = tensor.dim_size(dims - 1);
}

} // namespace


namespace ML {

ml_model Model::MakeHandle(Model* model)
{
    return reinterpret_cast<ml_model>(model);
}

Model* Model::FromHandle(ml_model model)
{
    return reinterpret_cast<Model*>(model);
}

Model::Model(ml_model_params const* params)
{
    if (params == nullptr)
    {
        throw std::runtime_error("Bad parameters argument");
    }

    if (params->model_path == nullptr)
    {
        throw std::runtime_error("Bad model_path model parameter value");
    }

    m_session.reset();

    auto status = tf::ReadBinaryProto(tf::Env::Default(), params->model_path, &m_graph_def);
    if (!status.ok())
    {
        m_error_cache << "Error reading graph definition: " << params->model_path << ": " << status;
        throw std::runtime_error(m_error_cache.str());
    }

    int input_node_idx = 0;
    int output_node_idx = m_graph_def.node_size() - 1;

    for (int i = 0; i < m_graph_def.node_size(); i++)
    {
        auto& node = m_graph_def.node(i);

        if (params->input_node != nullptr && node.name() == params->input_node)
        {
            input_node_idx = i;
            FillImageInfo(node, m_input_info);
        }

        if (params->output_node != nullptr && node.name() == params->output_node)
        {
            output_node_idx = i;
            FillImageInfo(node, m_output_info);
        }
    }

    FillImageInfo(m_graph_def.node(input_node_idx), m_input_info);
    FillImageInfo(m_graph_def.node(output_node_idx), m_output_info);

    m_input_node = m_graph_def.node(input_node_idx).name();

    m_output_nodes.clear();
    m_output_nodes.push_back(m_graph_def.node(output_node_idx).name());

    tf::Session* session;
    status = tf::NewSession(CreateSessionOptions(*params), &session);
    if (!status.ok())
    {
        m_error_cache << "Unable to start session: " << status;
        throw std::runtime_error(m_error_cache.str());
    }

    m_session.reset(session);

    status = session->Create(m_graph_def);
    if (!status.ok())
    {
        m_session.reset();
        m_error_cache << "Error creating graph: " << status;
        throw std::runtime_error(m_error_cache.str());
    }
}

ml_status Model::GetInfo(ml_image_info* input_info, ml_image_info* output_info)
{
    if (m_session == nullptr)
    {
        return ML_FAIL;
    }

    if (input_info != nullptr)
    {
        *input_info = m_input_info;
    }

    if (output_info != nullptr)
    {
        *output_info = m_output_info;
    }

    return ML_OK;
}

ml_status Model::SetInputInfo(ml_image_info const* info)
{
    m_error_cache.str("");

    if (info == nullptr)
    {
        m_error_cache << "Bad info parameter";
        return ML_FAIL;
    }

    if (m_input_info.dtype != info->dtype)
    {
        m_error_cache << "Overriding data type "
                      << m_input_info.dtype << " with " << info->dtype;
        return ML_FAIL;
    }

    auto validate_dim = [this, info](auto dim, char const* name)
    {
        if (m_input_info.*dim != 0 && info->*dim != m_input_info.*dim)
        {
            m_error_cache << "Overriding " << name << " dimension "
                          << m_input_info.*dim << " with " << info->*dim;
            return false;
        }
        return true;
    };

    if (!ForEachDim(validate_dim))
    {
        return ML_FAIL;
    }

    auto is_same_dim = [this, info](auto dim, char const* name)
    {
        return m_input_info.*dim == info->*dim;
    };

    if (ForEachDim(is_same_dim))
    {
        return ML_OK; // Nothing's changed
    }

    m_input_info = *info;

    auto dtype = DataTypeToTF(m_input_info.dtype);

    tf::TensorShape input_shape {
        1,
        static_cast<tf::int64>(m_input_info.height),
        static_cast<tf::int64>(m_input_info.width),
        static_cast<tf::int64>(m_input_info.channels)
    };

    m_input_map.clear();
    m_input_map.emplace_back(m_input_node, tf::Tensor(dtype, std::move(input_shape)));

    try
    {
        // Run inference in order to know exact output image dimensions
        Image input(info);
        if (!InferToCache(input))
        {
            return ML_FAIL;
        }

        FillImageInfo(m_output_cache.front(), m_output_info);
        return ML_OK;
    }
    catch (std::exception& e)
    {
        m_error_cache << e.what();
        return ML_FAIL;
    }
}

ml_status Model::Infer(ml_image input, ml_image output)
{
    m_error_cache.str("");

    if (ML::Image::FromHandle(input) == nullptr)
    {
        m_error_cache << "Bad input image handle";
        return ML_FAIL;
    }

    if (ML::Image::FromHandle(output) == nullptr)
    {
        m_error_cache << "Bad output image handle";
        return ML_FAIL;
    }

    ml_image_info output_info;
    ML::Image::FromHandle(output)->GetInfo(&output_info);

    auto validate_dim = [this, &output_info](auto dim, char const* name)
    {
        if (output_info.*dim != m_output_info.*dim)
        {
            m_error_cache << "Output image " << name << " dimension "
                << output_info.*dim << " does not match " << m_output_info.*dim;
            return false;
        }
        return true;
    };

    if (!ForEachDim(validate_dim))
    {
        return ML_FAIL;
    }

    if (!InferToCache(*ML::Image::FromHandle(input)))
    {
        return ML_FAIL;
    }

    size_t output_size;
    void* output_data = ML::Image::FromHandle(output)->Map(&output_size);

    tf::StringPiece tensor_data = m_output_cache.front().tensor_data();

    if (output_size != tensor_data.size())
    {
        ML::Image::FromHandle(output)->Unmap(output_data);

        m_error_cache << "Internal error: output size does not match: "
            << output_size << " vs " << tensor_data.size();
        return ML_FAIL;
    }

    std::memcpy(output_data, tensor_data.data(), output_size);
    ML::Image::FromHandle(output)->Unmap(output_data);
    return ML_OK;
}

char* Model::GetError(char* buffer, size_t buffer_size) const
{
    return FillBuffer(buffer, buffer_size, m_error_cache.str());
}

bool Model::InferToCache(Image& input)
{
    m_output_cache.clear(); // Invalidate previous data

    m_error_cache.str("");

    auto validate_dim = [this](auto dim, char const* name)
    {
        if (m_input_info.*dim == 0)
        {
            m_error_cache << "Input image " << name << " dimension is not specified";
            return false;
        }
        return true;
    };

    if (!ForEachDim(validate_dim))
    {
        return false;
    }

    size_t input_size;
    void* input_data = input.Map(&input_size);

    tf::StringPiece tensor_data = m_input_map.front().second.tensor_data();

    if (input_size != tensor_data.size())
    {
        input.Unmap(input_data);

        m_error_cache << "Internal error: input size does not match: "
                      << input_size << " vs " << tensor_data.size();
        return false;
    }

    std::memcpy(const_cast<char*>(tensor_data.data()), input_data, input_size);
    input.Unmap(input_data);

    auto status = m_session->Run(m_input_map, m_output_nodes, {}, &m_output_cache);
    if (!status.ok())
    {
        m_error_cache << "Inference error: " << status;
        return false;
    }

    return true;
}

} // namespace ML


char* mlGetModelError(ml_model model, char* buffer, size_t buffer_size)
{
    if (ML::Model::FromHandle(model) == nullptr)
    {
        return ML::FillBuffer(buffer, buffer_size, "Bad model handle");
    }

    return ML::Model::FromHandle(model)->GetError(buffer, buffer_size);
}

ml_status mlGetModelInfo(ml_model model, ml_image_info* input_info, ml_image_info* output_info)
{
    if (ML::Model::FromHandle(model) == nullptr)
    {
        return ML_FAIL;
    }

    return ML::Model::FromHandle(model)->GetInfo(input_info, output_info);
}

ml_status mlSetModelInputInfo(ml_model model, ml_image_info const* info)
{
    if (ML::Model::FromHandle(model) == nullptr)
    {
        return ML_FAIL;
    }

    return ML::Model::FromHandle(model)->SetInputInfo(info);
}

ml_status mlInfer(ml_model model, ml_image inputs, ml_image outputs)
{
    if (ML::Model::FromHandle(model) == nullptr)
    {
        return ML_FAIL;
    }

    return ML::Model::FromHandle(model)->Infer(inputs, outputs);
}

void mlReleaseModel(ml_model model)
{
    delete ML::Model::FromHandle(model);
}
