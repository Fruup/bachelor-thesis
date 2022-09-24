#include <engine/hzpch.h>

#include "Kernel.h"
#include "Dataset.h"

#define M_PI 3.14159265358979323846264338327950288

CubicSplineKernel::CubicSplineKernel(float h) : h(h) {}

float CubicSplineKernel::W(const glm::vec3& r) {
    const float sig_d = 8.0f / (M_PI * h * h * h);
    float q = glm::length(r) / h;

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

glm::vec3 CubicSplineKernel::gradW(const glm::vec3& r) {
    const float sig_d = 8.0f / (M_PI * h * h * h);
    float rn = glm::length(r);
    float q = rn / h;
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

AnisotropicKernel::AnisotropicKernel(float h, ::Dataset& dataset) :
    h(h), Dataset(dataset)
{
}

float AnisotropicKernel::W(const glm::vec3& r)
{
    // Do PCA here

    return 0.0f;
}
