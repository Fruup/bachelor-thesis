#pragma once

#include <engine/renderer/objects/VertexBuffer.h>
#include <engine/camera/Camera3D.h>
#include <engine/camera/CameraController3D.h>

#include "app/Dataset.h"
#include "BilateralBuffer.h"
#include "DepthRenderPass.h"
#include "CompositionRenderPass.h"
#include "GaussRenderPass.h"
#include "ShowImageRenderPass.h"

class Event;

class AdvancedRenderer
{
public:
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 UV;

		static const std::array<vk::VertexInputAttributeDescription, 2> Attributes;
		static const vk::VertexInputBindingDescription Binding;
	};

public:
	AdvancedRenderer();

	void Init(Dataset* dataset);
	void Exit();

	void Update(float time);
	void HandleEvent(Event& e);

	void Render();
	void RenderUI();
	
private:
	void CollectRenderData();

	void DrawDepthPass();
	void RayMarch();
	void DrawFullscreenQuad();

	float ComputeDensity(const glm::vec3& x);

public:
	DepthRenderPass DepthRenderPass;
	GaussRenderPass GaussRenderPass;
	CompositionRenderPass CompositionRenderPass;
	ShowImageRenderPass ShowImageRenderPass;

	BilateralBuffer DepthBuffer, SmoothedDepthBuffer;
	BilateralBuffer PositionsBuffer;
	BilateralBuffer NormalsBuffer;

	VertexBuffer VertexBuffer;

	Dataset* Dataset = nullptr;
	int CurrentFrame = 0;

	uint32_t NumVertices = 0;

	Camera3D Camera;
	CameraController3D CameraController;
};
