#pragma once

#include "app/Dataset.h"

#include <engine/camera/EditorCameraController3D.h>

class FluidRenderer
{
public:
	FluidRenderer() :
		CameraController(Camera)
	{
	}

	bool Init(const Dataset& dataset)
	{
		Dataset = dataset;

		return VInit();
	}

	void Exit()
	{
		VExit();
		Dataset.Exit();
	}

	void Update(float time)
	{
		CameraController.Update(time);

		VUpdate(time);
	}

	void HandleEvent(Event& e)
	{
		CameraController.HandleEvent(e);

		VHandleEvent(e);
	}

	virtual void RenderFrame() = 0;

protected:
	virtual bool VInit() = 0;
	virtual void VExit() = 0;

	virtual void VUpdate(float time) {}
	virtual void VHandleEvent(Event& e) {}

	Dataset Dataset;
	int CurrentFrame = 0;

	Camera3D Camera;
	EditorCameraController3D CameraController;
};
