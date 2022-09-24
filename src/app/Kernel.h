#pragma once

class Dataset;

class CubicSplineKernel
{
public:
    explicit CubicSplineKernel(float h);

    float W(const glm::vec3& r);

    glm::vec3 gradW(const glm::vec3& r);

private:
    float h;
};

class AnisotropicKernel
{
public:
    explicit AnisotropicKernel(float h, Dataset& dataset);

    float W(const glm::vec3& r);

private:
    float h;
    Dataset& Dataset;
};
