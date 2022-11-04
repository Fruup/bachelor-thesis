#pragma once

struct VisualizationSettings
{
	int Frame;

	int MaxSteps;
	float StepSize;
	float IsoDensity;

	bool EnableAnisotropy;

	float k_n;
	float k_r;
	float k_s;
	int N_eps;
};
