#include "Orbit.h"

namespace PathTracer
{
    Orbit::Orbit()
        : upside_down_(false)
        , inversion_angle_(0.0f)
        , eye_(0.0f, 1.0f, 1.0f)
        , center_(0.0f, 1.0f, 0.0f)
        , up_(0.0f, 1.0f, 0.0f)
    {
        UpdateInversionAngle();
    }

    Orbit::Orbit(const glm::vec3 &eye, const glm::vec3 &center)
        : upside_down_(false)
        , inversion_angle_(0.0f)
        , eye_(eye)
        , center_(center)
        , up_(0.0f, 1.0f, 0.0f)
    {
        UpdateInversionAngle();
    }

    Orbit::Orbit(const glm::vec3 &eye, const glm::vec3 &center, const glm::vec3 &up)
        : upside_down_(false)
        , inversion_angle_(0.0f)
        , eye_(eye)
        , center_(center)
        , up_(up)
    {
        UpdateInversionAngle();
    }
}