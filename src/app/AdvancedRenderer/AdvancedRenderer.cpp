#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>

// ------------------------------------------------------------------------

auto& Vulkan = Renderer::GetInstance();

// ------------------------------------------------------------------------
// PUBLIC FUNCTIONS

AdvancedRenderer::AdvancedRenderer() :
	DepthRenderPass(*this),
	CameraController(Camera)
{
}

void AdvancedRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	DepthBuffer.Init(BilateralBuffer::Depth);
	PositionsBuffer.Init(BilateralBuffer::Color);
	NormalsBuffer.Init(BilateralBuffer::Color);

	DepthRenderPass.Init();

	VertexBuffer.Create(6 * Dataset->MaxParticles);
}

void AdvancedRenderer::Exit()
{
	DepthRenderPass.Exit();

	NormalsBuffer.Exit();
	PositionsBuffer.Exit();
	DepthBuffer.Exit();
}

void AdvancedRenderer::Update(float time)
{
	CameraController.Update(time);
}

void AdvancedRenderer::HandleEvent(Event& e)
{
	CameraController.HandleEvent(e);
}

void AdvancedRenderer::Render()
{
	// - transition depth buffer
	// - collect render data
	// - begin depth pass
	// - render scene
	// - end depth pass
	// - transition depth buffer
	// - copy depth buffer to CPU
	// - do ray marching and fill positions and normals buffer
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// - begin composition pass
	// - render fullscreen quad
	// - end composition pass

	DepthBuffer.PrepareGPULayoutForRendering(Vulkan.CommandBuffer);

	CollectRenderData();

	DepthRenderPass.Begin();
	DrawDepthPass();
	DepthRenderPass.End();

	DepthBuffer.PrepareGPULayoutForCopying(Vulkan.CommandBuffer);
	DepthBuffer.CopyFromGPU();
	RayMarch();

	PositionsBuffer.CopyToGPU();
	NormalsBuffer.CopyToGPU();

	// CompositionPass.Begin();
	// DrawCompositionPass();
	// CompositionPass.End();
}

void AdvancedRenderer::RenderUI()
{
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void AdvancedRenderer::CollectRenderData()
{
	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	const glm::vec3 up(0, 1, 0);

	for (auto& particle : Dataset->Snapshots[CurrentFrame])
	{
		glm::vec3 z = CameraController.System[2];
		glm::vec3 x = glm::normalize(glm::cross(up, z));
		glm::vec3 y = glm::normalize(glm::cross(x, z));

		glm::vec3 p(particle[0], particle[1], particle[2]);

		glm::vec3 topLeft     = p + Dataset->ParticleRadius * (-x + y);
		glm::vec3 topRight    = p + Dataset->ParticleRadius * (+x + y);
		glm::vec3 bottomLeft  = p + Dataset->ParticleRadius * (-x - y);
		glm::vec3 bottomRight = p + Dataset->ParticleRadius * (+x - y);

		target[NumVertices++] = { topLeft, { 0, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { topRight, { 1, 0 } };
		target[NumVertices++] = { bottomLeft, { 0, 1 } };
		target[NumVertices++] = { bottomRight, { 1, 1 } };
	}

	Vulkan.Device.unmapMemory(VertexBuffer.BufferCPU.Memory);

	// copy
	if (NumVertices > 0)
	{
		OneTimeCommand cmd;
		VertexBuffer.Copy(cmd);
	}
}

void AdvancedRenderer::DrawDepthPass()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(NumVertices, 1, 0, 0);
}

void AdvancedRenderer::RayMarch()
{
	// ...
}

void AdvancedRenderer::DrawCompositionPass()
{
	Vulkan.CommandBuffer.draw(3, 1, 0, 0);
}

// ------------------------------------------------------------------------
