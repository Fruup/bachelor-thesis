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
#include "CoordinateSystemRenderPass.h"

#include "cuda/RayMarcher.cuh"
#include "app/ExternalImage.h"

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
	void DrawFullscreenQuad();

	void CreateSemaphores();

public:
	DepthRenderPass DepthRenderPass;
	//GaussRenderPass GaussRenderPass;
	CompositionRenderPass CompositionRenderPass;
	ShowImageRenderPass ShowImageRenderPass;
	CoordinateSystemRenderPass CoordinateSystemRenderPass;

	//BilateralBuffer DepthBuffer, SmoothedDepthBuffer;
	BilateralBuffer SmoothedDepthBuffer;

	ExternalImage DepthBuffer;
	ExternalImage PositionsBuffer;
	ExternalImage NormalsBuffer;

	VertexBuffer VertexBuffer;

	Dataset* Dataset = nullptr;

	uint32_t NumVertices = 0;

	Camera3D Camera;
	CameraController3D CameraController;

	CudaRayMarcher m_RayMarcher;

	vk::Semaphore m_SemaphoreVK;
	vk::Semaphore m_SemaphoreCU;
};
