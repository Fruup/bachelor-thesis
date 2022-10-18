#pragma once

class Dataset;

class CubicSplineKernel
{
public:
    explicit CubicSplineKernel(float h);

    float W(const glm::vec3& r);

    glm::vec3 gradW(const glm::vec3& r);

private:
    const float h;
    const float h_inv;
    const float sig_d;
};

class CubicKernel
{
public:
    explicit CubicKernel(float h);

    float W(const glm::vec3& r);

    //glm::vec3 gradW(const glm::vec3& r);

private:
    const float h;
    const float h_inv;
};
