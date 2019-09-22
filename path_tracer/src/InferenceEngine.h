#pragma once

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <dlfcn.h>

#include "model_holder/model_runner.h"
#include "model_holder/model_holder.h"
#include "model_holder/model_runner_wrappers.hpp"

template <typename T> using MLScopedObject = std::shared_ptr<std::remove_pointer_t<T>>;

namespace
{
void CheckContextStatus(ml_context context, bool status)
{
    if (!status)
    {
        std::vector<char> buffer(1024);
        throw std::runtime_error(mlGetContextError(context, buffer.data(), buffer.size()));
    }
}

void CheckModelStatus(ml_model model, bool status)
{
    if (!status)
    {
        std::vector<char> buffer(1024);
        throw std::runtime_error(mlGetModelError(model, buffer.data(), buffer.size()));
    }
}

template<class T>
MLScopedObject<T> MakeReleaser(T handle, void(* release_func)(T))
{
    auto release = [handle, release_func](void*) { release_func(handle); };
    return MLScopedObject<T>(handle, release);
}

}

namespace PathTracer
{
    class InferenceEngine
    {
        private:
            MLScopedObject<ml_context> context_releaser_;
            MLScopedObject<ml_model> model_releaser_;
            MLScopedObject<ml_image> output_image_releaser_;
            MLScopedObject<ml_image> input_image_releaser_;

            static std::vector<float> UnpackColor(std::vector<uint32_t> const& color)
            {
                std::vector<float> unpacked(color.size() * 3);
                for (size_t i = 0; i < color.size(); i++)
                {
                    unpacked[3 * i + 0] = float((color[i] >> 16) & 0xFFu) / 255.0f;
                    unpacked[3 * i + 1] = float((color[i] >> 8) & 0xFFu) / 255.0f;
                    unpacked[3 * i + 2] = float((color[i] >> 0) & 0xFFu) / 255.0f;
                }
            }

            static std::vector<uint32_t> PackColor(std::vector<float> const& color) 
            {
                std::vector<uint32_t> packed(color.size() / 3);
                for (size_t i = 0; i < packed.size(); i++)
                {
                    packed[i] =  255u << 24;
                    packed[i] += (uint32_t(color[3 * i + 0] * 255.0f) & 0xFFu) << 16;
                    packed[i] += (uint32_t(color[3 * i + 1] * 255.0f) & 0xFFu) << 8;
                    packed[i] += (uint32_t(color[3 * i + 2] * 255.0f) & 0xFFu) << 0;
                }
            }

        public:
        InferenceEngine(std::string path_to_model, uint32_t width, uint32_t height)
        {
            ml_context context = mlCreateContext();
            if (context == ML_INVALID_HANDLE)
            {
                throw std::runtime_error("Error creating context");
            }
            try
            {
                // Release the context in the end
                context_releaser_ = MakeReleaser(context, &mlReleaseContext);

                // Set model parameters
                ml_model_params params = {};
                params.model_path = path_to_model.c_str();
                params.input_node = nullptr;
                params.output_node = nullptr;

                // Create a model using the parameters
                ml_model model = mlCreateModel(context, &params);
                CheckContextStatus(context, model != nullptr);

                // Release the model in the end
                model_releaser_ = MakeReleaser(model, &mlReleaseModel);

                // Get partial input image information
                ml_image_info input_info;
                ml_image_info output_info;
                CheckModelStatus(model, mlGetModelInfo(model, &input_info, &output_info) == ML_OK);

                std::cerr << "Input (init): " << input_info.width << " x " << input_info.height
                        << " x " << input_info.channels << "\n";

                std::cerr << "Output (init): " << output_info.width << " x " << output_info.height
                        << " x " << output_info.channels << "\n";

                // Set unspecified input image dimensions
                input_info.width = width;
                input_info.height = height;
                std::cerr << "set_model_info" << std::endl;
                CheckModelStatus(model, mlSetModelInputInfo(model, &input_info) == ML_OK);

                // Get output image information
                CheckModelStatus(model, mlGetModelInfo(model, &input_info, &output_info) == ML_OK);

                std::cerr << "Input: " << input_info.width << " x " << input_info.height
                        << " x " << input_info.channels << "\n";

                std::cerr << "Output: " << output_info.width << " x " << output_info.height
                        << " x " << output_info.channels << "\n";

                std::cerr << "create image" << std::endl;
                // Create the input image
                ml_image input_image = mlCreateImage(context, &input_info);
                CheckContextStatus(context, input_image != ML_INVALID_HANDLE);

                // Release the input image in the end
                input_image_releaser_ = MakeReleaser(input_image, &mlReleaseImage);

                // Create the output image
                ml_image output_image = mlCreateImage(context, &output_info);
                CheckContextStatus(context, output_image != ML_INVALID_HANDLE);

                // Release the output image in the end
                output_image_releaser_ = MakeReleaser(output_image, &mlReleaseImage);
            }
            catch (std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }

        void Inference(std::vector<uint32_t>& color)
        {
            // Read input
            auto rgb_color = UnpackColor(color);

            // Fill the input image with data
            size_t input_size;
            void* input_data = mlMapImage(input_image_releaser_.get(), &input_size);
            if (rgb_color.size() * sizeof(float) != input_size)
            {
                throw std::runtime_error("Bad input size: " + std::to_string(rgb_color.size() * sizeof(float))
                                        + ", expected: " + std::to_string(input_size));
            }
            std::memcpy(input_data, rgb_color.data(), input_size);
            mlUnmapImage(input_image_releaser_.get(), input_data);

            // Run the inference
            CheckModelStatus(model_releaser_.get(), mlInfer(model_releaser_.get(), input_image_releaser_.get(), output_image_releaser_.get()) == ML_OK);

            // Get data from the output image
            size_t output_size;
            void* output_data = mlMapImage(output_image_releaser_.get(), &output_size);
            std::memcpy(rgb_color.data(), output_data, output_size);
            mlUnmapImage(output_image_releaser_.get(), output_data);

            color = PackColor(rgb_color);
        }

    };

}