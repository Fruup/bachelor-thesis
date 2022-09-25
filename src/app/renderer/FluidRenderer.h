#pragma once

#include "app/Dataset.h"

#include <engine/camera/CameraController3D.h>

class FluidRenderer
{
public:
	~FluidRenderer()
	{
		delete Dataset;
		Dataset = nullptr;
	}

	void SetDataset(Dataset* dataset)
	{
		Dataset = dataset;
	}

protected:
	Dataset* Dataset;
	int CurrentFrame = 100;
};
