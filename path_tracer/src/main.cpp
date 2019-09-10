#include "Application.h"

int main(int argc, char const** argv)
{
    auto &app = PathTracer::Application::GetAppInstance();
    return app.Run(argc, argv);
}