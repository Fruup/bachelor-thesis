#pragma once

#include "app/Dataset.h"

#include <engine/camera/EditorCameraController3D.h>

class FluidRenderer
{
public:
	~FluidRenderer()
	{
		Dataset.Exit();
	}

	void SetDataset(const Dataset& dataset)
	{
		Dataset = dataset;
	}

protected:
	Dataset Dataset;
	int CurrentFrame = 0;
};
