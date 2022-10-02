#pragma once

#include <engine/renderer/objects/VertexBuffer.h>
#include <engine/camera/Camera3D.h>
#include <engine/camera/CameraController3D.h>

#include "app/Dataset.h"
#include "DiskRenderPass.h"
#include "../AdvancedRenderer/CoordinateSystemRenderPass.h"
#include "../AdvancedRenderer/BilateralBuffer.h"

class Event;

class DiskRenderer
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
	DiskRenderer();

	void Init(Dataset* dataset);
	void Exit();

	void Update(float time);
	void HandleEvent(Event& e);

	void Render();
	void RenderUI();

private:
	void CollectRenderData();

	void DrawParticles();

	float ComputeDensity(const glm::vec3& x);

public:
	DiskRenderPass DiskRenderPass;
	//CoordinateSystemRenderPass CoordinateSystemRenderPass;

	BilateralBuffer DepthBuffer;

	VertexBuffer VertexBuffer;

	Dataset* Dataset = nullptr;
	int CurrentFrame = 0;

	uint32_t NumVertices = 0;

	Camera3D Camera;
	CameraController3D CameraController;
};
