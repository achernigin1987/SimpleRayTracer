# Model Runner

## 1. Model C API

The API is annotated with Doxygen comments, see [model_runner.h](model_runner.h) file.

### Usage example

```C++
    // Create a context
    ml_context context = mlCreateContext();
    if (context == ML_INVALID_HANDLE)
    {
        // Handle error...
    }

    // Set model parameters
    ml_model_params params = {};
    params.model_path = "path/to/model/protobuf/file";
    // and possibly set other parameters...

    // Create a model using the parameters
    ml_model model = mlCreateModel(context, &params);
    if (model == ML_INVALID_HANDLE)
    {
        // Handle error...

        // The error message for any context error can be retrieved this way
        char message[1024];
        printf("%s\n", mlGetContextError(context, message, sizeof(message));
    }

    // Get partial input image information
    ml_image_info input_info;
    if (mlGetModelInfo(model, &input_info, NULL) != ML_OK)
    {
        // Handle error...

        // The error message for any model error can be retrieved this way
        char message[1024];
        printf("%s\n", mlGetModelError(model, message, sizeof(message));
    }

    // Set unspecified input image dimensions
    input_info.width = width;
    input_info.height = height;
    if (mlSetModelInputInfo(model, &input_info) != ML_OK)
    {
        // Handle error...
    }

    // Get output image information
    ml_image_info output_info;
    if (mlGetModelInfo(model, NULL, &output_info) != ML_OK)
    {
        // Handle error...
    }

    // Create the input image
    ml_image input_image = mlCreateImage(context, &input_info);
    if (input_image == ML_INVALID_HANDLE)
    {
        // Handle error...
    }

    // Create the output image
    ml_image output_image = mlCreateImage(context, &output_info);
    if (output_image == ML_INVALID_HANDLE)
    {
        // Handle error...
    }

    // Get an input data buffer pointer and size
    size_t input_size;
    void* input_data = mlMapImage(input_image, &input_size);
    if (input_data == NULL)
    {
        // Handle error...
    }

    // Fill in the input data [input_data, input_data + input_size)...

    mlUnmapImage(input_image, input_data);

    // Run inference
    if (mlInfer(model, input_image, output_image) != ML_OK)
    {
        // Handle error...
    }

    // Get an output data buffer pointer and size
    size_t output_size;
    void* output_data = mlMapImage(output_image, &output_size);
    if (output_data == NULL)
    {
        // Handle error...
    }

    // Use the output data [output_data, output_data + output_size)...

    mlUnmapImage(output_image, output_data);

    // Release the input and output images
    mlReleaseImage(input_image);
    mlReleaseImage(output_image);

    // Release the model
    mlReleaseModel(model);

    // Release the context
    mlReleaseContext(context);
```

## 2. Building the model runner library

The TensorFlow `bazel` environment is used for builing.

Follow [this TensorFlow instruction](https://www.tensorflow.org/install/install_sources):

* Install all the required prerequisites.

* Clone the TensorFlow repository (original one or the ROCm fork).

* Checkout the required TensorFlow version, e.g. `git checkout v1.14`.

* Run the `./configure` script.

* Copy the `model_runner` directory into the TensorFlow repository root.

* Run from the repository root:
```bash
bazel build --config=opt --config=monolithic //model_runner:libRadeonProML.so
```

The built library is `bazel-bin/model_runner/libModelRunner.so`.

## 4. Building and running the test application

Follow the instructions for the library above (the last step may be omitted).

To build a simple test application, run:
```bash
bazel build ---config=opt --config=monolithic //model_runner:test_app
```
* For ROCm stack, pass an extra argument:
```bash
bazel build --config=opt --config=monolithic --config=rocm //model_runner:test_app
```

The built application is `bazel-bin/model_runner/test_app`.

The application requires `libRadeonProML.so` to be accessible for loading
e.g. through setting `LD_LIBRARY_PATH`:
```bash
export LD_LIBRARY_PATH=bazel-bin/model_runner
```

To run the test_app from the tensorflow directory:

```bash
bazel-bin/model_runner/test_app -w 800 -h 600 \
    -m color_only_denoiser.pb \
    -i input.bin -o output.bin
```

Run the application with a `-help` argument to get the list of available options:
```
     -gmf: Amount of GPU memory to use (0, 1], unset by default
     -w: Input image width
     -h: Input image height
     -m: Path to TensorFlow model (protobuf format)
     -i: File with input data, read data from stdin if omitted
     -o: File for output data, write to stdout if omitted
     -in: Input node name, autodetect if omitted
     -on: Output node name, autodetect if omitted
     -vdl: Comma-separated list of device indices to use, use all devices if omitted
```

The `-gmf` parameter limits the amount of GPU memory to use.

The input must contain contiguous data of a 3D image with dimensions expected by a model.
