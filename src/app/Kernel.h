#pragma once

class Dataset;

class CubicSplineKernel
{
public:
    CubicSplineKernel() = default;
    CubicSplineKernel(float h);

    float W(const glm::vec3& r);
    glm::vec3 gradW(const glm::vec3& r);

private:
    float h;
    float h_squared;
    float h_inv;
    float sig_d;
};

class AnisotropicKernel
{
public:
    AnisotropicKernel() = default;
    AnisotropicKernel(float h);

    float W(const glm::mat3& G,
            const float detG,
            const glm::vec3& r);

    glm::vec3 gradW(const glm::mat3& G,
                    const float detG,
                    const glm::vec3& r);

private:
    float h;
    float h_squared;
    float h_inv;
    float sig;
};

class CubicKernel
{
public:
    CubicKernel() = default;
    CubicKernel(float h);

    float W(const glm::vec3& r);

    //glm::vec3 gradW(const glm::vec3& r);

private:
    float h;
    float h_inv;
};
