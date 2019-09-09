#include "ml.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif


class ArgParser
{
public:
    template<class T>
    void AddArg(T* value, const std::string& name, std::string help, bool optional = false)
    {
        std::unique_ptr<ArgImpl<T>> arg(new ArgImpl<T>);
        arg->name = "-" + name;
        arg->help = std::move(help);
        arg->value = value;
        arg->has_value = optional;
        args_.insert(std::make_pair("-" + name, std::move(arg)));
    }

    void Parse(int argc, char* argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] == std::string("-help"))
            {
                throw std::runtime_error(HelpString());
            }

            if (argv[i][0] == '-')
            {
                if (i % 2 != 1)
                {
                    throw std::runtime_error("Missing option value: " + std::string(argv[i - 1]));
                }
            }
            else
            {
                if (i % 2 != 0)
                {
                    throw std::runtime_error("Missing option name: " + std::string(argv[i])
                                             + "\n" + HelpString());
                }
                auto arg = args_.find(argv[i - 1]);
                if (arg == args_.end())
                {
                    throw std::runtime_error("Unknown option: " + std::string(argv[i - 1])
                                             + "\n" + HelpString());
                }
                arg->second->Parse(argv[i]);
            }
        }

        for (auto& arg : args_)
        {
            if (!arg.second->has_value)
            {
                throw std::runtime_error("Missing option: " + arg.second->name
                                         + "\n" + HelpString());
            }
        }
    }

private:
    struct Arg
    {
        virtual void Parse(const std::string& value) = 0;

        std::string name;
        std::string help;
        bool has_value = false;
    };

    template<class T>
    struct ArgImpl : Arg
    {
        void Parse(const std::string& string) override
        {
            std::istringstream stream(string);
            stream >> *value;
            if (stream.fail() && ! stream.eof())
            {
                throw std::runtime_error("Bad parameter " + name + ": " + string);
            }
            has_value = true;
        }

        T* value = nullptr;
    };

    std::string HelpString() const
    {
        std::ostringstream stream;
        stream << "Available options:\n";
        for (auto& arg : args_)
        {
            stream << std::setw(5) << std::setfill(' ') << "" << arg.second->name
                << ": " << arg.second->help << "\n";
        }
        return stream.str();
    }

    std::map<std::string, std::unique_ptr<Arg>> args_;
};


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

std::string ReadInput(const std::string& input_file)
{
    std::istream* input_stream;
    std::ifstream input_file_stream;

    if (input_file.empty())
    {
        freopen(nullptr, "rb", stdin);
        input_stream = &std::cin;
        std::cerr << "Reading data from stdin...\n";
    }
    else
    {
        input_file_stream.open(input_file, std::ios_base::binary);
        if (input_file_stream.fail())
        {
            throw std::runtime_error(std::string("Error reading ") + input_file);
        }
        input_stream = &input_file_stream;
        std::cerr << "Reading data from file: " << input_file << "\n";
    }

    std::ostringstream stream;
    stream << input_stream->rdbuf();

    auto input = stream.str();
    std::cerr << "Input data size: " << input.size() << " bytes\n";
    return input;
}

void WriteOutput(const std::string& output_file, const std::string& output)
{
    std::cerr << "Output data size: " << output.size() << " bytes\n";

    std::ostream* output_stream;
    std::ofstream output_file_stream;

    if (output_file.empty())
    {
        freopen(nullptr, "wb", stdout);
        output_stream = &std::cout;
        std::cerr << "Writing result to stdout\n";
    }
    else
    {
        output_file_stream.open(output_file, std::ios_base::binary);
        if (output_file_stream.fail())
        {
            throw std::runtime_error(std::string("Error writing ") + output_file);
        }
        output_stream = &output_file_stream;
        std::cerr << "Writing result to file: " << output_file << "\n";
    }

    output_stream->write(output.data(), output.size());
}


template<class T>
auto MakeReleaser(T handle, void(* release_func)(T))
{
    auto release = [handle, release_func](void*) { release_func(handle); };
    std::unique_ptr<void, decltype(release)> releaser(&handle, release);
    return releaser;
}


int main(int argc, char* argv[])
try
{
    ArgParser parser;

    std::string model_path;
    parser.AddArg(&model_path, "m", "Path to TensorFlow model (protobuf format)");

    std::string input_node;
    parser.AddArg(&input_node, "in", "Input node name, autodetect if omitted", true);

    std::string output_node;
    parser.AddArg(&output_node, "on", "Output node name, autodetect if omitted", true);

    std::string input_file;
    parser.AddArg(&input_file, "i", "File with input data, read data from stdin if omitted", true);

    std::string output_file;
    parser.AddArg(&output_file, "o", "File for output data, write to stdout if omitted", true);

    std::size_t width = 0;
    parser.AddArg(&width, "w", "Input image width");

    std::size_t height = 0;
    parser.AddArg(&height, "h", "Input image height");

    float gpu_memory_fraction = 0;
    parser.AddArg(&gpu_memory_fraction,
        "gmf", "Amount of GPU memory to use (0, 1], unset by default", true);

    std::string visible_devices;
    parser.AddArg(&visible_devices,
        "vdl", "Comma-separated list of device indices to use, use all devices if omitted", true);

    parser.Parse(argc, argv);

    std::cerr << "Model path: " << model_path << "\n";

    if (gpu_memory_fraction > 0)
    {
        std::cerr << "GPU memory fraction: " << gpu_memory_fraction << "\n";
    }
    if (!visible_devices.empty())
    {
        std::cerr << "Visible GPU devices: " << visible_devices << "\n";
    }

    // Create a context
    ml_context context = mlCreateContext();
    if (context == ML_INVALID_HANDLE)
    {
        throw std::runtime_error("Error creating context");
    }

    // Release the context in the end
    auto context_releaser = MakeReleaser(context, &mlReleaseContext);

    // Set model parameters
    ml_model_params params = {};
    params.model_path = model_path.c_str();
    params.input_node = input_node.empty() ? nullptr : input_node.c_str();
    params.output_node = output_node.empty() ? nullptr : output_node.c_str();
    params.gpu_memory_fraction = gpu_memory_fraction;
    params.visible_devices = visible_devices.empty() ? nullptr : visible_devices.c_str();

    // Create a model using the parameters
    ml_model model = mlCreateModel(context, &params);
    CheckContextStatus(context, model != nullptr);

    // Release the model in the end
    auto model_releaser = MakeReleaser(model, &mlReleaseModel);

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
    CheckModelStatus(model, mlSetModelInputInfo(model, &input_info) == ML_OK);

    // Get output image information
    CheckModelStatus(model, mlGetModelInfo(model, &input_info, &output_info) == ML_OK);

    std::cerr << "Input: " << input_info.width << " x " << input_info.height
              << " x " << input_info.channels << "\n";

    std::cerr << "Output: " << output_info.width << " x " << output_info.height
              << " x " << output_info.channels << "\n";

    // Create the input image
    ml_image input_image = mlCreateImage(context, &input_info);
    CheckContextStatus(context, input_image != ML_INVALID_HANDLE);

    // Release the input image in the end
    auto input_image_releaser = MakeReleaser(input_image, &mlReleaseImage);

    // Create the output image
    ml_image output_image = mlCreateImage(context, &output_info);
    CheckContextStatus(context, output_image != ML_INVALID_HANDLE);

    // Release the output image in the end
    auto output_image_releaser = MakeReleaser(output_image, &mlReleaseImage);

    // Read input
    auto input = ReadInput(input_file);

    // Fill the input image with data
    size_t input_size;
    void* input_data = mlMapImage(input_image, &input_size);
    if (input.size() != input_size)
    {
        throw std::runtime_error("Bad input size: " + std::to_string(input.size())
                                 + ", expected: " + std::to_string(input_size));
    }
    std::memcpy(input_data, input.data(), input_size);
    mlUnmapImage(input_image, input_data);

    // Run the inference
    CheckModelStatus(model, mlInfer(model, input_image, output_image) == ML_OK);

    // Get data from the output image
    size_t output_size;
    void* output_data = mlMapImage(output_image, &output_size);
    std::string output(static_cast<char*>(output_data), output_size);
    mlUnmapImage(output_image, output_data);

    // Write the output
    WriteOutput(output_file, output);
}
catch (std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return -1;
}
