#pragma once


#define _USE_MATH_DEFINES
#include <math.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace PathTracer
{
    class Orbit
    {
    public:
        Orbit();
        Orbit(const glm::vec3 &eye, const glm::vec3 &center);
        Orbit(const glm::vec3 &eye, const glm::vec3 &center, const glm::vec3 &up);

        inline void MoveForward(float distance);
        inline void MovePerpendicular(float distanceX, float distanceY);
        inline void Rotate(float radiansX, float radiansY);

        inline glm::vec3 Eye() const
        {
            return eye_;
        }
        inline glm::vec3 Center() const
        {
            return center_;
        }
        inline glm::vec3 Up() const
        {
            return (upside_down_ ? -1.0f : 1.0f) * up_;
        }

    protected:
        inline void UpdateInversionAngle()
        {
            inversion_angle_ = acosf(glm::dot(glm::normalize(eye_ - center_), up_));
        }

        bool        upside_down_;
        float       inversion_angle_;
        glm::vec3   eye_;
        glm::vec3   center_;
        glm::vec3   up_;
    };

    void Orbit::MoveForward(float distance)
    {
        auto distanceToCenter = std::max(glm::length(eye_ - center_) + distance, 0.01f);
        eye_ = center_ + glm::normalize(eye_ - center_) * distanceToCenter;
        UpdateInversionAngle();
    }

    void Orbit::MovePerpendicular(float distanceX, float distanceY)
    {
        auto forwardMotionVector = glm::normalize(eye_ - center_);
        auto horizontalMotionVector = glm::normalize(glm::cross((up_ * (upside_down_ ? -1.0f : 1.0f)), forwardMotionVector));
        auto verticalMotionVector = glm::normalize(glm::cross(horizontalMotionVector, forwardMotionVector));
        center_ = center_ - horizontalMotionVector * distanceX + verticalMotionVector * distanceY;
        eye_ = eye_ - horizontalMotionVector * distanceX + verticalMotionVector * distanceY;
        UpdateInversionAngle();
    }

    void Orbit::Rotate(float radiansX, float radiansY)
    {
        glm::vec3 viewDirection;
        glm::quat rotationQuaternion, positionQuaternion;

        // Detect camera inversion and update the up vector accordingly
        auto upside_down = upside_down_;
        auto inversionAngle = inversion_angle_ + radiansY * (upside_down_ ? -1.0f : 1.0f);
        if (inversionAngle < 0.0f || inversionAngle > M_PI)
            upside_down_ = !upside_down_;

        // Calculate the rotation and position quaternions for vertical motion
        viewDirection = center_ - eye_;
        rotationQuaternion = glm::angleAxis(radiansY, glm::normalize(glm::cross(viewDirection, up_)) * (upside_down ? -1.0f : 1.0f));
        positionQuaternion = glm::angleAxis(static_cast<float>(M_PI), viewDirection);

        // Make sure we are not degenerating our up vector at that point
        auto eye = center_ - glm::axis(rotationQuaternion * positionQuaternion * glm::inverse(rotationQuaternion));
        if (glm::length(glm::normalize(center_ - eye) - up_) > 0.001f &&
            (glm::length(glm::normalize(center_ - eye) + up_) > 0.001f))
            eye_ = eye;
        else
            upside_down_ = upside_down;

        // Calculate the rotation and position quaternions for horizontal motion
        viewDirection = center_ - eye_;
        rotationQuaternion = glm::angleAxis(-radiansX, up_);
        positionQuaternion = glm::angleAxis(static_cast<float>(M_PI), viewDirection);
        eye_ = center_ - glm::axis(rotationQuaternion * positionQuaternion * glm::inverse(rotationQuaternion));

        // Update the inversion angle
        UpdateInversionAngle();
    }
}
