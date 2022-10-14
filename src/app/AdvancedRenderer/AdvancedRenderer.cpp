#include <engine/hzpch.h>

#include "AdvancedRenderer.h"

#include <engine/renderer/Renderer.h>
#include <engine/renderer/objects/Command.h>
#include <engine/events/Event.h>
#include <engine/utils/PerformanceTimer.h>

#include "app/Kernel.h"
#include "app/Utils.h"

#include <fstream>
#include <thread>

// ------------------------------------------------------------------------

struct VisualizationSettings
{
	int Frame;

	int MaxSteps;
	float StepSize;
	float IsoDensity;

	bool EnableAnisotropy;
};

VisualizationSettings g_VisualizationSettings = {
	.MaxSteps = 32,
	.StepSize = 0.007f,
	.IsoDensity = 400.0f,
};

static float s_ProcessTimer = 0.0f;
constexpr const float s_ProcessTimerMax = 1.0f;

std::thread RayMarchThread;
std::atomic_bool RayMarchFinished = true;
std::condition_variable ConditionVariable;
std::mutex Mutex;
bool ShouldTerminateThread = false;

bool s_EnableDepthPass = true;
bool s_EnableRayMarch = false;
bool s_EnableGaussPass = true;
bool s_EnableCoordinateSystem = true;
bool s_EnableComposition = true;
bool s_EnableShowDepth = false;

// ------------------------------------------------------------------------

decltype(AdvancedRenderer::Vertex::Attributes) AdvancedRenderer::Vertex::Attributes = {
	// uint32_t location
	// uint32_t binding
	// Format format = Format::eUndefined
	// uint32_t offset
	vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
	vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, UV) },
};

const int bla = offsetof(AdvancedRenderer::Vertex, UV);

decltype(AdvancedRenderer::Vertex::Binding) AdvancedRenderer::Vertex::Binding = vk::VertexInputBindingDescription{
	0, // uint32_t binding
	sizeof(AdvancedRenderer::Vertex), // uint32_t stride
	vk::VertexInputRate::eVertex, // VertexInputRate inputRate
};

// ------------------------------------------------------------------------

template <typename T>
void DumpDataToFile(const char* filename, const std::vector<T>& data)
{
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	file.write((char*)data.data(), sizeof(T) * data.size());
	file.close();
}

void DumpDataToFile(const char* filename, const void* data, size_t size)
{
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	file.write((char*)data, size);
	file.close();
}

// ------------------------------------------------------------------------
// PUBLIC FUNCTIONS

AdvancedRenderer::AdvancedRenderer() :
	DepthRenderPass(*this),
	CompositionRenderPass(*this),
	GaussRenderPass(*this),
	ShowImageRenderPass(*this),
	CoordinateSystemRenderPass(*this),
	CameraController(Camera)
{
}

void AdvancedRenderer::Init(::Dataset* dataset)
{
	Dataset = dataset;

	DepthBuffer.Init(BilateralBuffer::Depth);
	SmoothedDepthBuffer.Init(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
							 vk::Format::eR32Sfloat,
							 vk::ImageAspectFlagBits::eColor);
	PositionsBuffer.Init(BilateralBuffer::Color);
	NormalsBuffer.Init(BilateralBuffer::Color);

	DepthRenderPass.Init();
	CompositionRenderPass.Init();
	GaussRenderPass.Init();
	CoordinateSystemRenderPass.Init();

	ShowImageRenderPass.ImageView = DepthBuffer.GPU.ImageView;
	ShowImageRenderPass.Sampler = DepthBuffer.GPU.Sampler;
	ShowImageRenderPass.Init();

	VertexBuffer.Create(6 * Dataset->MaxParticles * sizeof(Vertex));

	// init camera
	Camera = Camera3D(
		glm::radians(60.0f),
		Vulkan.GetAspect(),
		0.1f,
		1000.0f
	);

	CameraController.ComputeMatrices();

	// start worker thread
	RayMarchThread = std::thread(&AdvancedRenderer::RayMarch, this);
}

void AdvancedRenderer::Exit()
{
	ShouldTerminateThread = true;
	ConditionVariable.notify_all();
	if (RayMarchThread.joinable())
		RayMarchThread.join();

	delete Dataset;
	VertexBuffer.Destroy();

	CoordinateSystemRenderPass.Exit();
	ShowImageRenderPass.Exit();
	GaussRenderPass.Exit();
	CompositionRenderPass.Exit();
	DepthRenderPass.Exit();

	NormalsBuffer.Exit();
	PositionsBuffer.Exit();
	SmoothedDepthBuffer.Exit();
	DepthBuffer.Exit();
}

void AdvancedRenderer::Update(float time)
{
	if (s_EnableRayMarch)
		s_ProcessTimer += time;

	CameraController.Update(time);
}

void AdvancedRenderer::HandleEvent(Event& e)
{
	CameraController.HandleEvent(e);
}

void AdvancedRenderer::Render()
{
	// - begin command buffer
	// 
	// - transition depth buffer
	// - collect render data
	// 
	// - begin depth pass
	// - render scene
	// - end depth pass
	// 
	// - begin gauss pass
	// - render gauss pass
	// - end gauss pass
	// 
	// - end command buffer
	// - submit command buffer
	// 
	// - transition depth buffer
	// - copy depth buffer to CPU
	// - do ray marching and fill positions and normals buffer
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// - (transition positions and normals buffer)
	// - copy positions and normals buffer to GPU
	// 
	// - begin command buffer
	// 
	// - begin composition pass
	// - render fullscreen quad
	// - end composition pass
	// 
	// - render ImGui pass
	// 
	// - end command buffer
	// - submit command buffer

	//   Done in Renderer:
	// begin command buffer
	
	{
		PROFILE_SCOPE("Pre-marching");

		CollectRenderData();

		if (s_EnableDepthPass)
		{
			DepthBuffer.TransitionLayout(vk::ImageLayout::eDepthAttachmentOptimal,
										 vk::AccessFlagBits::eDepthStencilAttachmentRead |
										 vk::AccessFlagBits::eDepthStencilAttachmentWrite,
										 vk::PipelineStageFlagBits::eEarlyFragmentTests |
										 vk::PipelineStageFlagBits::eLateFragmentTests);

			DepthRenderPass.Begin();
			DrawDepthPass();
			DepthRenderPass.End();
		}

		if (s_EnableGaussPass)
		{
			DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eColorAttachmentRead,
										 vk::PipelineStageFlagBits::eColorAttachmentOutput);

			SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eColorAttachmentOptimal,
												 vk::AccessFlagBits::eColorAttachmentWrite,
												 vk::PipelineStageFlagBits::eColorAttachmentOutput);

			GaussRenderPass.Begin();
			DrawFullscreenQuad();
			GaussRenderPass.End();
		}

		DepthBuffer.TransitionLayout(vk::ImageLayout::eTransferSrcOptimal,
									 vk::AccessFlagBits::eTransferRead,
									 vk::PipelineStageFlagBits::eTransfer);

		PositionsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
										 vk::AccessFlagBits::eTransferWrite,
										 vk::PipelineStageFlagBits::eTransfer);

		NormalsBuffer.TransitionLayout(vk::ImageLayout::eTransferDstOptimal,
									   vk::AccessFlagBits::eTransferWrite,
									   vk::PipelineStageFlagBits::eTransfer);

		Vulkan.CommandBuffer.end();

		{
			PROFILE_SCOPE("Submit and wait");

			Vulkan.Submit();
			Vulkan.WaitForRenderingFinished();
		}

		if (RayMarchFinished)
			DepthBuffer.CopyFromGPU();
	}

	if (RayMarchFinished && s_ProcessTimer >= s_ProcessTimerMax)
	{
		RayMarchFinished = false;

		ConditionVariable.notify_all();
	}

	{
		PROFILE_SCOPE("Post-marching");

		if (RayMarchFinished)
		{
			PositionsBuffer.CopyToGPU();
			NormalsBuffer.CopyToGPU();
		}

		Vulkan.CommandBuffer.reset();
		vk::CommandBufferBeginInfo beginInfo;
		Vulkan.CommandBuffer.begin(beginInfo);

		PositionsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
										 vk::AccessFlagBits::eShaderRead,
										 vk::PipelineStageFlagBits::eFragmentShader);

		NormalsBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									   vk::AccessFlagBits::eShaderRead,
									   vk::PipelineStageFlagBits::eFragmentShader);

		DepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
									 vk::AccessFlagBits::eShaderRead,
									 vk::PipelineStageFlagBits::eFragmentShader);

		SmoothedDepthBuffer.TransitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal,
											 vk::AccessFlagBits::eShaderRead,
											 vk::PipelineStageFlagBits::eFragmentShader);

		TransitionImageLayout(Vulkan.SwapchainImages[Vulkan.CurrentImageIndex],
							  vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  {},
							  vk::AccessFlagBits::eColorAttachmentWrite,
							  vk::PipelineStageFlagBits::eTopOfPipe,
							  vk::PipelineStageFlagBits::eColorAttachmentOutput);

		if (s_EnableShowDepth)
		{
			ShowImageRenderPass.Begin();
			DrawFullscreenQuad();
			ShowImageRenderPass.End();
		}

		if (s_EnableComposition)
		{
			CompositionRenderPass.Begin();
			DrawFullscreenQuad();
			CompositionRenderPass.End();
		}

		if (s_EnableCoordinateSystem)
		{
			CoordinateSystemRenderPass.Begin();
			CoordinateSystemRenderPass.End();
		}
	}

	//   Done in Renderer:
	// Transition swapchain image layout
	// Render ImGui pass
	// end command buffer
}

void AdvancedRenderer::RenderUI()
{
	ImGui::Begin("Renderer");

	ImGui::DragInt("Frame", &g_VisualizationSettings.Frame, 0.125f, 0, Dataset->Snapshots.size() - 1, "%d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SliderInt("# steps", &g_VisualizationSettings.MaxSteps, 0, 64);
	ImGui::DragFloat("Step size", &g_VisualizationSettings.StepSize, 0.001f, 0.001f, 1.0f, "%f");
	ImGui::DragFloat("Iso", &g_VisualizationSettings.IsoDensity, 0.1f, 0.001f, 1000.0f, "%f", ImGuiSliderFlags_Logarithmic);

	ImGui::Checkbox("Anisotropy2", &g_VisualizationSettings.EnableAnisotropy);

	ImGui::SliderFloat("Blur spread", &GaussRenderPass.Spread, 1.0f, 32.0f);
	
	// does not make sense because hash grid is not recomputed
	// ImGui::DragFloat("Particle radius", &Dataset->ParticleRadius, 0.01f, 0.1f, 3.0f);

	{
		ImGui::Separator();
		ImGui::Text("Enable render passes");

		ImGui::Checkbox("Depth", &s_EnableDepthPass);
		ImGui::Checkbox("Coordinate System", &s_EnableCoordinateSystem);
		ImGui::Checkbox("Gaussian blur", &s_EnableGaussPass);
		ImGui::Checkbox("Ray march", &s_EnableRayMarch);
	}

	{
		ImGui::Separator();
		ImGui::Text("What to show");

		ImGui::TreePush("last_render_pass");

		static int radio = 0;
		ImGui::RadioButton("Composition", &radio, 0);
		
		if (ImGui::RadioButton("Depth", &radio, 1))
		{
			ShowImageRenderPass.ImageView = DepthBuffer.GPU.ImageView;
			ShowImageRenderPass.Sampler = DepthBuffer.GPU.Sampler;
		}

		if (ImGui::RadioButton("Blurred depth", &radio, 2))
		{
			ShowImageRenderPass.ImageView = SmoothedDepthBuffer.GPU.ImageView;
			ShowImageRenderPass.Sampler = SmoothedDepthBuffer.GPU.Sampler;
		}

		ImGui::TreePop();

		if (radio == 0)
		{
			s_EnableComposition = true;
			s_EnableShowDepth = false;
		}
		else if (radio == 1 || radio == 2)
		{
			s_EnableComposition = false;
			s_EnableShowDepth = true;
		}
	}

	ImGui::End();

	GaussRenderPass.RenderUI();
}

// ------------------------------------------------------------------------
// PRIVATE HELPER FUNCTIONS

void AdvancedRenderer::CollectRenderData()
{
	PROFILE_FUNCTION();

	NumVertices = 0;

	Vertex* target = reinterpret_cast<Vertex*>(
		Vulkan.Device.mapMemory(VertexBuffer.BufferCPU.Memory, 0, VertexBuffer.BufferCPU.Size));

	for (auto& particle : Dataset->Snapshots[g_VisualizationSettings.Frame])
	{
		glm::vec3 x = CameraController.System[0];
		glm::vec3 y = CameraController.System[1];

		glm::vec3 p = particle;
		const float R = Dataset->ParticleRadius;

		glm::vec3 topLeft     = p + R * (-x + y);
		glm::vec3 topRight    = p + R * (+x + y);
		glm::vec3 bottomLeft  = p + R * (-x - y);
		glm::vec3 bottomRight = p + R * (+x - y);

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

glm::vec3 eigenToGlm(const Eigen::Vector3f& v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

void PCA_Exact(const glm::vec3* data,
			   size_t n,
			   glm::mat3& G,
			   float particleRadius)
{
	/* Exact PCA using Eigen */

	// prepare data points
	Eigen::MatrixX3f X;
	X.resize(n, Eigen::NoChange);

	for (size_t i = 0; i < n; i++)
	{
		X(i, 0) = data[i].x;
		X(i, 1) = data[i].y;
		X(i, 2) = data[i].z;
	}

	// prepare weights
	// TODO: use different kernel here (see YT13)
	Eigen::MatrixXf L;
	L.resize(n, n);

	CubicSplineKernel K(particleRadius);

	for (size_t i = 0; i < n; i++)
	{
		glm::vec3 x_i = data[i];

		for (size_t j = i; j < n; j++)
		{
			glm::vec3 x_j = data[j];
			float d_ij;

			if (i == j)
			{
				d_ij = 0.0f;
				for (size_t j2 = 0; j2 < n; j2++)
				{
					glm::vec3 x_j2 = data[j2];
					d_ij += K.W(x_i - x_j2);
				}
			}
			else d_ij = -K.W(x_i - x_j);

			L(i, j) = d_ij;
			L(j, i) = d_ij;
		}
	}

	// decompose
	Eigen::Matrix3f C = X.transpose() * L * X;
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(C);

	if (solver.info() != Eigen::Success)
	{
		SPDLOG_ERROR("Failed to compute eigenvectors!");
		return;
	}

	//SPDLOG_INFO("{} {} {}", solver.eigenvalues()[0], solver.eigenvalues()[1], solver.eigenvalues()[2]);

	auto S_inv = solver.eigenvalues().asDiagonal().inverse();
	Eigen::Matrix3f R = solver.eigenvectors();

	Eigen::Matrix3f G_ = (1.0f / particleRadius) * R * S_inv * R.transpose();

	for (size_t i = 0; i < 3; i++)
		G[i] = {
			G_(0, i),
			G_(1, i),
			G_(2, i),
		};

	//SPDLOG_INFO("{} {} {}", G[0][0], G[0][1], G[0][2]);
}

void PCA(const glm::vec3* data, size_t n, glm::vec3& r)
{
	// https://en.wikipedia.org/wiki/Principal_component_analysis
	// "Covariance/free computation"

	// start with random vector
	const float theta = RandomFloat(0.0f, glm::two_pi<float>());
	const float phi = RandomFloat(-glm::half_pi<float>(), glm::half_pi<float>());
	const float cosPhi = cos(phi);
	r = glm::vec3(sin(theta) * cosPhi, sin(phi), cos(theta) * cosPhi);

	const size_t N = 64;

	for (size_t i = 0; i < N; i++)
	{
		glm::vec3 s(0);
		for (uint32_t j = 0; j < n; j++)
		{
			const glm::vec3 x = data[j];
			s += glm::dot(x, r) * x;
		}

		const glm::vec3 error = glm::dot(r, s) * r - s;

		r = glm::normalize(s);

		if (glm::dot(error, error) < 0.000001f)
		{
			//SPDLOG_WARN("Break at {}/{}.", i, N);
			return;
		}
	}
}

void AdvancedRenderer::RayMarch()
{
	VisualizationSettings visualizationSettings;

	while (!ShouldTerminateThread)
	{
		{
			std::unique_lock lock(Mutex);
			ConditionVariable.wait(lock);
		}

		PROFILE_FUNCTION();

		memcpy(&visualizationSettings, &g_VisualizationSettings, sizeof(visualizationSettings));
		//visualizationSettings = g_VisualizationSettings;

		const int width = Vulkan.SwapchainExtent.width;
		const int height = Vulkan.SwapchainExtent.height;
		const float widthF = float(width);
		const float heightF = float(height);

		const float* depth = reinterpret_cast<float*>(DepthBuffer.MapCPUMemory());
		glm::vec4* positions = reinterpret_cast<glm::vec4*>(PositionsBuffer.MapCPUMemory());
		glm::vec4* normals = reinterpret_cast<glm::vec4*>(NormalsBuffer.MapCPUMemory());

		const auto& particles = Dataset->Snapshots[visualizationSettings.Frame];
		const glm::mat4 invProjectionView = Camera.GetInvProjectionView();
		const glm::vec3 cameraPosition = CameraController.Position;

		CubicSplineKernel K(Dataset->ParticleRadius);

		#pragma omp parallel for
		for (int y = 0; y < height; y++)
		{
			#pragma omp parallel for
			for (int x = 0; x < width; x++)
			{
				uint32_t index = y * width + x;
				const float z = depth[index];

				positions[index] = glm::vec4(0);
				normals[index] = glm::vec4(0);
				if (z == 1.0f) continue;

				glm::vec3 clip(2.0f * float(x) / widthF - 1.0f,
							   2.0f * float(y) / heightF - 1.0f,
							   z);

				glm::vec4 worldH = invProjectionView * glm::vec4(clip, 1);
				//glm::vec3 world = glm::vec3(worldH) / worldH.w;

				glm::vec3 position = glm::vec3(worldH) / worldH.w;
				glm::vec3 step = visualizationSettings.StepSize * glm::normalize(position - cameraPosition);

				for (int i = 0; i < visualizationSettings.MaxSteps; i++)
				{
					// step
					position += step;

					//glm::vec3 pc(0);
					//glm::mat3 G;
					float density = 0.0f;

					const auto& neighbors = Dataset->GetNeighbors(position, visualizationSettings.Frame);

#if 0
					if (visualizationSettings.EnableAnisotropy)
					{
						glm::vec3 neighborsMean(0);
						std::vector<glm::vec3> neighborPositions(neighbors.size());
						for (size_t i = 0; i < neighborPositions.size(); i++)
						{
							neighborPositions[i] = particles[neighbors[i]];
							neighborsMean += neighborPositions[i];
						}
						neighborsMean /= float(neighborPositions.size());

						// normalize mean to 0
						for (size_t i = 0; i < neighborPositions.size(); i++)
							neighborPositions[i] -= neighborsMean;

						// find primary axis
						//PCA(neighborPositions.data(), neighborPositions.size(), pc);
							PCA_Exact(neighborPositions.data(), neighborPositions.size(), G, Dataset->ParticleRadius);
					}
#endif

					// cannot use 'neighborPositions' here as it is shifted to mean 0
					for (const auto& index : neighbors)
					{
						const glm::vec3& neighborPosition = particles[index];
						const glm::vec3 r = neighborPosition - position;
						//const float correction = glm::dot(r, pc);

						//density += K.W(r - s_CorrectionCoeff * correction * pc);
						//density += K.W(r);
						/*if (visualizationSettings.EnableAnisotropy)
							density += K.W(glm::determinant(G) * G * r);
						else*/
							density += K.W(r);
					}

					if (density >= visualizationSettings.IsoDensity)
					{
						// set position
						positions[index] = glm::vec4(position, 1);

#if 1
						// compute object normal
						glm::vec3 normal(0);

						for (const auto& index : neighbors)
						{
							const glm::vec3& neighborPosition = particles[index];
							const glm::vec3 r = neighborPosition - position;

							normal += K.gradW(r);
						}

						normals[index] = glm::vec4(glm::normalize(normal), 1);
#endif

						i = visualizationSettings.MaxSteps;
					}
				}
			}
		}

		NormalsBuffer.UnmapCPUMemory();
		PositionsBuffer.UnmapCPUMemory();
		DepthBuffer.UnmapCPUMemory();

		s_ProcessTimer = 0.0f;
		RayMarchFinished = true;
	}
}

void AdvancedRenderer::DrawFullscreenQuad()
{
	vk::DeviceSize offset = 0;
	Vulkan.CommandBuffer.bindVertexBuffers(
		0,
		VertexBuffer.BufferGPU.Buffer,
		offset);

	Vulkan.CommandBuffer.draw(3, 1, 0, 0);
}

// ------------------------------------------------------------------------
