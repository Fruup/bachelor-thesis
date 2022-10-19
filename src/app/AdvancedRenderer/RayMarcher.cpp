#include <engine/hzpch.h>

#include "RayMarcher.h"

#include "app/StackArray.h"
#include "app/Dataset.h"

#include <engine/renderer/Renderer.h>
#include <engine/utils/PerformanceTimer.h>

// ---------------------------------------------------------

#define MAX_NEIGHBORS 3072

// ---------------------------------------------------------

glm::vec3 eigenToGlm(const Eigen::Vector3f& v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

Eigen::Vector3f glmToEigen(const glm::vec3& v)
{
	return Eigen::Vector3f(v.x, v.y, v.z);
}

// ---------------------------------------------------------

RayMarcher::RayMarcher() :
	m_ThreadPool(std::thread::hardware_concurrency() - 1)
{
}

void RayMarcher::Exit()
{
	m_ThreadPool.Exit();
}

void RayMarcher::Prepare(const VisualizationSettings& settings,
						 const CameraController3D& camera,
						 Dataset* dataset,
						 glm::vec4* positions,
						 glm::vec4* normals,
						 float* depth)
{
	m_Settings = settings;
	m_Dataset = dataset;

	m_Width = Vulkan.SwapchainExtent.width;
	m_Height = Vulkan.SwapchainExtent.height;
	m_TwoWidthInv = 2.0f / float(Vulkan.SwapchainExtent.width);
	m_TwoHeightInv = 2.0f / float(Vulkan.SwapchainExtent.height);

	m_Positions = positions;
	m_Normals = normals;
	m_Depth = depth;

	m_CameraPosition = camera.Position;
	m_InvProjectionView = camera.Camera.GetInvProjectionView();

	m_IsotropicKernel = CubicSplineKernel(m_Dataset->ParticleRadius);
	m_AnisotropicKernel = AnisotropicKernel(m_Dataset->ParticleRadius);
}

void RayMarcher::Start()
{
	if (m_Settings.EnableAnisotropy)
		m_ThreadPool.SetFunction(
			std::bind(&RayMarcher::PerPixel_Anisotropic, this, std::placeholders::_1));
	else
		m_ThreadPool.SetFunction(
			std::bind(&RayMarcher::PerPixel_Isotropic, this, std::placeholders::_1));

	m_ThreadPool.Start(m_Width * m_Height);
}

void RayMarcher::WPCA(const glm::vec3& particle,
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
	CubicKernel kernel(m_Dataset->ParticleRadiusExt);

	for (size_t j = 0; j < N; j++)
	{
		const glm::vec3& x_j = neighborPositions[j];
		glm::vec3 r = x_j - particle;

		float w = kernel.W(r);
		weights[j] = w;

		weightSum += w;
		mean += w * x_j;
	}

	const float invWeightSum = 1.0f / weightSum;
	mean *= invWeightSum;

	// compute C
	Eigen::Matrix3f C;
	C.setZero();

	for (size_t j = 0; j < N; j++)
	{
		const glm::vec3& x_j = neighborPositions[j];

		Eigen::Vector3f x = glmToEigen(x_j - mean);
		C += weights[j] * x * x.transpose();
	}

	C *= invWeightSum;

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
	const float k_n = m_Settings.k_n;
	const float k_r = m_Settings.k_r;
	const float k_s = m_Settings.k_s;
	const uint32_t N_eps = m_Settings.N_eps;

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
		m_Dataset->ParticleRadiusInv * /*R **/ Sigma.cwiseInverse().asDiagonal() * R.transpose();

	//std::cout << G_Eigen << std::endl;

	G = {
		eigenToGlm(G_Eigen.col(0)),
		eigenToGlm(G_Eigen.col(1)),
		eigenToGlm(G_Eigen.col(2)),
	};
}

void RayMarcher::PerPixel_Isotropic(uint32_t index)
{
	const float z = m_Depth[index];

	m_Positions[index] = glm::vec4(0);
	m_Normals[index] = glm::vec4(0);
	if (z == 1.0f) return;

	const auto& particles = m_Dataset->Frames[m_Settings.Frame];

	const glm::vec3 clip(float(index % m_Width) * m_TwoWidthInv - 1.0f,
						 float(index / m_Width) * m_TwoHeightInv - 1.0f,
						 z);

	const glm::vec4 worldH = m_InvProjectionView * glm::vec4(clip, 1);
	glm::vec3 position = glm::vec3(worldH) / worldH.w;
	const glm::vec3 step = m_Settings.StepSize * glm::normalize(position - m_CameraPosition);

	for (int i = 0; i < m_Settings.MaxSteps; i++)
	{
		// step
		position += step;

		const auto& neighbors = m_Dataset->GetNeighborsExt(position, m_Settings.Frame);
		const auto N = neighbors.size();

		// store neighbor positions on stack
		constexpr uint32_t Capacity = MAX_NEIGHBORS;
		HZ_ASSERT(Capacity >= N, "Neighborhood capacity too small!");

		StackArray<glm::vec3, Capacity> neighborPositionsAbsExt;
		StackArray<glm::vec3, Capacity> neighborPositionsRel;
		neighborPositionsAbsExt.resize(N);

		for (uint32_t i = 0; i < N; i++)
		{
			neighborPositionsAbsExt[i] = particles[neighbors[i]];

			const glm::vec3 r = neighborPositionsAbsExt[i] - position;

			if (glm::dot(r, r) < m_Dataset->ParticleRadius * m_Dataset->ParticleRadius)
				neighborPositionsRel.push(r);
		}

		// density
		float density = 0.0f;

		for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
			density += m_IsotropicKernel.W(neighborPositionsRel[i]);

		if (density >= m_Settings.IsoDensity)
		{
			// set position
			m_Positions[index] = glm::vec4(position, 1);

			// compute object normal
			glm::vec3 normal(0);

			for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
				normal += m_IsotropicKernel.gradW(neighborPositionsRel[i]);

			m_Normals[index] = glm::vec4(glm::normalize(normal), 1);

			// break
			i = m_Settings.MaxSteps;
		}
	}
}

void RayMarcher::PerPixel_Anisotropic(uint32_t index)
{
	const float z = m_Depth[index];

	m_Positions[index] = glm::vec4(0);
	m_Normals[index] = glm::vec4(0);
	if (z == 1.0f) return;

	const auto& particles = m_Dataset->Frames[m_Settings.Frame];

	const glm::vec3 clip(float(index % m_Width) * m_TwoWidthInv - 1.0f,
						 float(index / m_Width) * m_TwoHeightInv - 1.0f,
						 z);

	const glm::vec4 worldH = m_InvProjectionView * glm::vec4(clip, 1);
	glm::vec3 position = glm::vec3(worldH) / worldH.w;
	const glm::vec3 step = m_Settings.StepSize * glm::normalize(position - m_CameraPosition);

	for (int i = 0; i < m_Settings.MaxSteps; i++)
	{
		// step
		position += step;

		const auto& neighbors = m_Dataset->GetNeighborsExt(position, m_Settings.Frame);
		const auto N = neighbors.size();

		// store neighbor positions on stack
		constexpr uint32_t Capacity = MAX_NEIGHBORS;
		HZ_ASSERT(Capacity >= N, "Neighborhood capacity too small!");

		StackArray<glm::vec3, Capacity> neighborPositionsAbsExt;
		StackArray<glm::vec3, Capacity> neighborPositionsRel;
		neighborPositionsAbsExt.resize(N);

		for (uint32_t i = 0; i < N; i++)
		{
			neighborPositionsAbsExt[i] = particles[neighbors[i]];

			const glm::vec3 r = neighborPositionsAbsExt[i] - position;

			if (glm::dot(r, r) < m_Dataset->ParticleRadius * m_Dataset->ParticleRadius)
				neighborPositionsRel.push(r);
		}

		// compute G and density
		glm::mat3 G;
		float density = 0.0f;

		WPCA(position, neighborPositionsAbsExt.data(), N, G);
		const float detG = glm::determinant(G);

		for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
			density += detG * m_AnisotropicKernel.W(G, detG, neighborPositionsRel[i]);

		if (density >= m_Settings.IsoDensity)
		{
			// set position
			m_Positions[index] = glm::vec4(position, 1);

			// compute object normal
			glm::vec3 normal(0);

			for (uint32_t i = 0; i < neighborPositionsRel.size(); i++)
				normal += detG * m_AnisotropicKernel.gradW(G, detG, neighborPositionsRel[i]);

			m_Normals[index] = glm::vec4(glm::normalize(normal), 1);

			// break
			i = m_Settings.MaxSteps;
		}
	}
}
