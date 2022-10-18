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

	float k_n;
	float k_r;
	float k_s;
	int N_eps;
};

VisualizationSettings g_VisualizationSettings = {
	.MaxSteps = 32,
	.StepSize = 0.007f,
	.IsoDensity = 400.0f,

	.EnableAnisotropy = false,
	.k_n = 0.5f,
	.k_r = 4.0f,
	.k_s = 1400.0f,
	.N_eps = 25,
};

VisualizationSettings g_VisualizationSettingsThread;

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

#define MAX_NEIGHBORS 4096

// ------------------------------------------------------------------------

template <typename value_t, uint32_t Capacity>
class StackArray
{
public:
	StackArray() = default;
	StackArray(uint32_t size): m_Size(size) {}

public:
	void resize(uint32_t size)
	{
		m_Size = size;
	}

	void push(const value_t& v)
	{
#if !PRODUCTION
		HZ_ASSERT(m_Size < Capacity, "Capacity exceeded!");
#endif

		m_Data[m_Size++] = v;
	}

	uint32_t size() { return m_Size; }
	value_t* data() { return m_Data; }

	value_t& operator [](uint32_t i) { return m_Data[i]; }

private:
	value_t m_Data[Capacity];
	uint32_t m_Size = 0;
};

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

	ImGui::DragInt("Frame", &g_VisualizationSettings.Frame, 0.125f, 0, Dataset->Frames.size() - 1, "%d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SliderInt("# steps", &g_VisualizationSettings.MaxSteps, 0, 64);
	ImGui::DragFloat("Step size", &g_VisualizationSettings.StepSize, 0.001f, 0.001f, 1.0f, "%f");
	ImGui::DragFloat("Iso", &g_VisualizationSettings.IsoDensity, 0.1f, 0.001f, 1000.0f, "%f", ImGuiSliderFlags_Logarithmic);

	ImGui::SliderFloat("Blur spread", &GaussRenderPass.Spread, 1.0f, 32.0f);
	
	// does not make sense because hash grid is not recomputed
	// ImGui::DragFloat("Particle radius", &Dataset->ParticleRadius, 0.01f, 0.1f, 3.0f);

	{
		ImGui::Separator();
		ImGui::Text("Anisotropy");

		ImGui::Checkbox("Enable", &g_VisualizationSettings.EnableAnisotropy);

		ImGui::DragFloat("k_n", &g_VisualizationSettings.k_n, 0.001f, 0.0f, 2.0f);
		ImGui::DragFloat("k_r", &g_VisualizationSettings.k_r, 0.001f, 0.0f, 8.0f);
		ImGui::DragFloat("k_s", &g_VisualizationSettings.k_s, 1.0f, 0.0f, 2000.0f);
		ImGui::DragInt("N_eps", &g_VisualizationSettings.N_eps, 1.0f, 1, 100);
	}

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

	for (auto& particle : Dataset->Frames[g_VisualizationSettings.Frame])
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

Eigen::Vector3f glmToEigen(const glm::vec3& v)
{
	return Eigen::Vector3f(v.x, v.y, v.z);
}

void AdvancedRenderer::WPCA(const glm::vec3& particle,
							const glm::vec3* neighborPositions,
							uint32_t N,
							glm::mat3& G)
{
	/* Agenda of this function:
	* 
	* - get neighbors in extended neighborhood
	* - compute weighted mean of neighbors
	* - construct weighted covariance matrix C of neighbor positions
	* - compute eigenvalues and -vectors of C
	* 
	*/

	// get neighbors
	/*const auto& particles = Dataset->Frames[g_VisualizationSettings.Frame];
	auto neighborIndices = Dataset->GetNeighborsExt(particle, g_VisualizationSettings.Frame);
	const auto N = neighborIndices.size();
	std::vector<glm::vec3> neighborPositions(N);

	for (size_t i = 0; i < N; i++)
		neighborPositions[i] = particles[neighborIndices[i]];*/

	// compute weights and mean
	StackArray<float, MAX_NEIGHBORS> weights(N);

	float weightSum = 0.0f;
	glm::vec3 mean(0);
	CubicKernel kernel(Dataset->ParticleRadiusExt);

	for (size_t j = 0; j < N; j++)
	{
		const glm::vec3& x_j = neighborPositions[j];
		glm::vec3 r = x_j - particle;

		float w = kernel.W(r);
		weights[j] = w;

		weightSum += w;
		mean += w * x_j;
	}

	mean /= weightSum;

	// compute C
	Eigen::Matrix3f C;
	C.setZero();

	for (size_t j = 0; j < N; j++)
	{
		const glm::vec3& x_j = neighborPositions[j];

		Eigen::Vector3f x = glmToEigen(x_j - mean);
		C += weights[j] * x * x.transpose();
	}

	C /= weightSum;

	// eigendecomposition
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver;
	solver.computeDirect(C);

	if (solver.info() != Eigen::Success)
	{
		SPDLOG_ERROR("Eigen decomposition failed!");
		return;
	}

	Eigen::Matrix3f R = solver.eigenvectors();
	Eigen::Vector3f Sigma;

	// prevent extreme deformations for singular matrices
	// eq. 15 [YT13]
	const float k_n = g_VisualizationSettingsThread.k_n;
	const float k_r = g_VisualizationSettingsThread.k_r;
	const float k_s = g_VisualizationSettingsThread.k_s;
	const uint32_t N_eps = g_VisualizationSettingsThread.N_eps;

	if (N <= N_eps)
		Sigma.setConstant(k_n);
	else
	{
		Sigma = solver.eigenvalues();

		const float MAX = Sigma[0] / k_r;
		
		for (size_t k = 1; k < Sigma.size(); k++)
			Sigma[k] = std::max(Sigma[k], MAX);

		Sigma *= k_s;
	}

	// leave out first R because of eq. 10 in [000]
	Eigen::Matrix3f G_Eigen =
		Dataset->ParticleRadiusInv * R * Sigma.cwiseInverse().asDiagonal() * R.transpose();

	//std::cout << G_Eigen << std::endl;

	G = {
		eigenToGlm(G_Eigen.col(0)),
		eigenToGlm(G_Eigen.col(1)),
		eigenToGlm(G_Eigen.col(2)),
	};
}

void AdvancedRenderer::RayMarch()
{
	VisualizationSettings& visualizationSettings = g_VisualizationSettingsThread;
	
	while (!ShouldTerminateThread)
	{
		{
			std::unique_lock lock(Mutex);
			ConditionVariable.wait(lock);
		}

		PROFILE_FUNCTION();

		visualizationSettings = g_VisualizationSettings;

		const int width = Vulkan.SwapchainExtent.width;
		const int height = Vulkan.SwapchainExtent.height;
		const float widthInv = 2.0f / float(width);
		const float heightInv = 2.0f / float(height);

		const float* depth = reinterpret_cast<float*>(DepthBuffer.MapCPUMemory());
		glm::vec4* positions = reinterpret_cast<glm::vec4*>(PositionsBuffer.MapCPUMemory());
		glm::vec4* normals = reinterpret_cast<glm::vec4*>(NormalsBuffer.MapCPUMemory());

		const auto& particles = Dataset->Frames[visualizationSettings.Frame];
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

				glm::vec3 clip(float(x) * widthInv - 1.0f,
							   float(y) * heightInv - 1.0f,
							   z);

				glm::vec4 worldH = invProjectionView * glm::vec4(clip, 1);
				glm::vec3 position = glm::vec3(worldH) / worldH.w;
				glm::vec3 step = visualizationSettings.StepSize * glm::normalize(position - cameraPosition);

				for (int i = 0; i < visualizationSettings.MaxSteps; i++)
				{
					// step
					position += step;

					float density = 0.0f;

					const auto& neighbors = Dataset->GetNeighborsExt(position, visualizationSettings.Frame);
					const auto N = neighbors.size();

					// store neighbor positions on stack
					constexpr uint32_t Capacity = 4096;
					HZ_ASSERT(Capacity >= N, "Neighborhood capacity too small!");

					StackArray<glm::vec3, Capacity> neighborPositionsAbsExt;
					StackArray<glm::vec3, Capacity> neighborPositionsRel;
					neighborPositionsAbsExt.resize(N);

					for (uint32_t i = 0; i < N; i++)
					{
						neighborPositionsAbsExt[i] = particles[neighbors[i]];

						const glm::vec3 r = neighborPositionsAbsExt[i] - position;

						if (glm::dot(r, r) < Dataset->ParticleRadius * Dataset->ParticleRadius)
							neighborPositionsRel.push(r);
					}

					// compute G and density
					glm::mat3 G;
					float detG;

					if (visualizationSettings.EnableAnisotropy)
					{
						WPCA(position, neighborPositionsAbsExt.data(), N, G);
						detG = glm::determinant(G);

						for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
							density += detG * K.W(G * neighborPositionsRel[i]);
					}
					else
						for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
							density += K.W(neighborPositionsRel[i]);


					if (density >= visualizationSettings.IsoDensity)
					{
						// set position
						positions[index] = glm::vec4(position, 1);

						// compute object normal
						glm::vec3 normal(0);
						
						if (visualizationSettings.EnableAnisotropy)
							for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
								normal += detG * K.gradW(G * neighborPositionsRel[i]);
						else
							for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
								normal += K.gradW(neighborPositionsRel[i]);

						normals[index] = glm::vec4(glm::normalize(normal), 1);

						// break
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
