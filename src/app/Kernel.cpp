#include <engine/hzpch.h>

#include "Kernel.h"
#include "Dataset.h"

// -------------------------------------------------------------------

CubicSplineKernel::CubicSplineKernel(float h) :
    h(h),
    h_squared(h * h),
    h_inv(1.0f / h),
    sig_d(8.0f / (glm::pi<float>() * h * h * h))
{
}

float CubicSplineKernel::W(const glm::vec3& r)
{
    float q = glm::dot(r, r);

    if (q >= h_squared)
        return 0.0f;

    q = glm::sqrt(q) * h_inv;

    if (q >= 0.5f)
    {
        const float q_ = 1.0f - q;
        return sig_d * (2.0f * q_ * q_ * q_);
    }
    
    return sig_d * (6.0f * (q * q * q - q * q) + 1.0f);
}

glm::vec3 CubicSplineKernel::gradW(const glm::vec3& r)
{
    const float rn = glm::dot(r, r);

    if (rn >= h_squared)
        return glm::vec3(0);

    const float r_length = glm::sqrt(rn);
    const float q = r_length * h_inv;
    const glm::vec3 gradQ = glm::normalize(r) / (r_length * h);

    if (q >= 0.5f)
    {
        float q_ = 1.0f - q;
        return -sig_d * gradQ * (6.0f * q_ * q_);
    }

    return sig_d * gradQ * (6.0f * (3.0f * q * q - 2.0f * q));
}
// -------------------------------------------------------------------

AnisotropicKernel::AnisotropicKernel(float h) :
    h(h),
    h_squared(h * h),
    h_inv(1.0f / h),
    sig(8.0f / glm::pi<float>())
{
}

float AnisotropicKernel::W(const glm::mat3& G,
                           const float detG,
                           const glm::vec3& _r)
{
    const glm::vec3 r = G * _r;

    float q = glm::dot(r, r);

    if (q >= h_squared)
        return 0.0f;

    q = glm::sqrt(q) * h_inv;

    if (q >= 0.5f)
    {
        const float q_ = 1.0f - q;
        return sig * detG * (2.0f * q_ * q_ * q_);
    }

    return sig * detG * (6.0f * (q * q * q - q * q) + 1.0f);
}

glm::vec3 AnisotropicKernel::gradW(const glm::mat3& G,
                                   const float detG,
                                   const glm::vec3& _r)
{
    const glm::vec3 r = G * _r;

    const float rn = glm::dot(r, r);

    if (rn >= h_squared)
        return glm::vec3(0);

    const float r_length = glm::sqrt(rn);
    const float q = r_length * h_inv;
    const glm::vec3 gradQ = glm::normalize(r) / (r_length * h);

    if (q >= 0.5f)
    {
        float q_ = 1.0f - q;
        return -sig * detG * gradQ * (6.0f * q_ * q_);
    }

    return sig * detG * gradQ * (6.0f * (3.0f * q * q - 2.0f * q));
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
