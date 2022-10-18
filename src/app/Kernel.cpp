#include <engine/hzpch.h>

#include "Kernel.h"
#include "Dataset.h"

CubicSplineKernel::CubicSplineKernel(float h) :
    h(h),
    h_inv(1.0f / h),
    sig_d(8.0f / (glm::pi<float>() * h * h * h))
{
}

float CubicSplineKernel::W(const glm::vec3& r)
{
    float q = glm::length(r) * h_inv;

    if (0.0 <= q && q <= 0.5) {
        return sig_d * (6 * (q * q * q - q * q) + 1);
    }
    else if (0.5 < q && q <= 1.0) {
        float q_ = 1 - q;
        return sig_d * (2 * q_ * q_ * q_);
    }
    else {
        return 0;
    }
}

glm::vec3 CubicSplineKernel::gradW(const glm::vec3& r)
{
    float rn = glm::length(r);
    float q = rn * h_inv;
    glm::vec3 gradQ = r / (rn * h);

    if (0.0 <= q && q <= 0.5) {
        return sig_d * gradQ * (6 * (3 * q * q - 2 * q));
    }
    else if (0.5 < q && q <= 1.0) {
        float q_ = 1.0f - q;
        return -sig_d * gradQ * (6 * q_ * q_);
    }
    else {
        return { 0, 0, 0 };
    }
}

// -------------------------------------------------------------------

CubicKernel::CubicKernel(float h) :
    h(h),
    h_inv(1.0f / h)
{
}

float CubicKernel::W(const glm::vec3& r)
{
    float d = glm::length(r);
    
    if (d >= h) return 0.0f;
    
    float k = d * h_inv;
    return 1.0f - k * k * k;
}
