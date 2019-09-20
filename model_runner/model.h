#pragma once

#include "model_runner.h"

#include "tensorflow/core/public/session.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>


namespace ML {

class Image;

class Model
{
public:
    static ml_model MakeHandle(Model* model);
    static Model* FromHandle(ml_model model);

    explicit Model(ml_model_params const* params);

    ml_status GetInfo(ml_image_info* input_info, ml_image_info* output_info);
    ml_status SetInputInfo(ml_image_info const* info);
    ml_status Infer(ml_image input, ml_image output);
    char* GetError(char* buffer, size_t buffer_size) const;

private:
    bool InferToCache(Image& input);

    std::string m_input_node;
    tensorflow::GraphDef m_graph_def;
    ml_image_info m_input_info;
    ml_image_info m_output_info;
    std::vector<std::pair<std::string, tensorflow::Tensor>> m_input_map;
    std::vector<std::string> m_output_nodes;
    std::unique_ptr<tensorflow::Session> m_session;
    std::vector<tensorflow::Tensor> m_output_cache;
    std::ostringstream m_error_cache;
};

} // namespace ML
