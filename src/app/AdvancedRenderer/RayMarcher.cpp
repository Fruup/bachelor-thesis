#include <engine/hzpch.h>

#include "RayMarcher.h"

#include "app/StackArray.h"
#include "app/Dataset.h"

#include <engine/renderer/Renderer.h>
#include <engine/utils/PerformanceTimer.h>

// ---------------------------------------------------------

//#define MAX_NEIGHBORS 3072
#define MAX_NEIGHBORS 2*4096

struct ThreadLocals
{
	uint32_t NumNeighborsExt;
	glm::vec3 NeighborPositions_AbsExt[MAX_NEIGHBORS];

	uint32_t NumNeighbors;
	glm::vec3 NeighborPositions_Rel[MAX_NEIGHBORS];
};

// ---------------------------------------------------------

glm::vec3 eigenToGlm(const Eigen::Vector3f& v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

Eigen::Vector3f glmToEigen(const glm::vec3& v)
{
	return Eigen::Vector3f(v.x, v.y, v.z);
}

Eigen::Matrix3f glmToEigen(const glm::mat3& m)
{
	Eigen::Matrix3f r;

	r.col(0) = glmToEigen(m[0]);
	r.col(1) = glmToEigen(m[1]);
	r.col(2) = glmToEigen(m[2]);

	return r;
}

// https://github.com/erich666/GraphicsGems/blob/master/gems/RayBox.c

// https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
glm::vec3 intersectAABB(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::vec3 boxMin, glm::vec3 boxMax) {
	glm::vec3 tMin = (boxMin - rayOrigin) / rayDir;
	glm::vec3 tMax = (boxMax - rayOrigin) / rayDir;
	glm::vec3 t1 = glm::min(tMin, tMax);
	glm::vec3 t2 = glm::max(tMin, tMax);
	float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
	float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);

	float t = std::max(tNear, tFar);

	return t >= 0.0f ? rayOrigin + rayDir * t : rayOrigin;
};

// ---------------------------------------------------------

RayMarcher::RayMarcher() :
	m_ThreadPool(std::thread::hardware_concurrency() - 1, sizeof(ThreadLocals))
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
	auto f = m_Settings.EnableAnisotropy ?
		&RayMarcher::PerPixel_Anisotropic :
		&RayMarcher::PerPixel_Isotropic;

	m_ThreadPool.SetFunction(
		std::bind(f, this, std::placeholders::_1, std::placeholders::_2));

	m_ThreadPool.Start(m_Width * m_Height);
}

void RayMarcher::WPCA(const glm::vec3& particle,
					  const glm::vec3* neighborPositions,
					  uint32_t N,
					  glm::mat3& G,
					  bool verbose)
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

	if (verbose)
	{
		std::cout << "C:" << std::endl << C << std::endl;
	}

	// eigendecomposition
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver;
	solver.computeDirect(C);

	if (solver.info() != Eigen::Success)
	{
		SPDLOG_ERROR("Eigen decomposition failed!");
		return;
	}

	// sort eigenvalues
	Eigen::Vector3f Sigma = solver.eigenvalues();
	Eigen::Matrix3f R = solver.eigenvectors();

#if 0
	// No need to sort. Determining maximum eigenvalue is sufficient.
	for (uint32_t i = 0; i < 3; i++)
	{
		for (uint32_t j = 0; j < 3 - i - 1; j++)
		{
			if (Sigma[j] < Sigma[j + 1])
			{
				const float tmp = Sigma(j);
				Sigma(j) = Sigma(j + 1);
				Sigma(j + 1) = tmp;

				const Eigen::Vector3f tmp2 = R.col(j);
				R.col(j) = R.col(j + 1);
				R.col(j + 1) = tmp2;
			}
		}
	}
#endif

#if 0
	const size_t j = 1;

	const float tmp = Sigma(j);
	Sigma(j) = Sigma(j + 1);
	Sigma(j + 1) = tmp;

	const Eigen::Vector3f tmp2 = R.col(j);
	R.col(j) = R.col(j + 1);
	R.col(j + 1) = tmp2;
#endif

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
		const float MINIMUM = std::max(Sigma[0], std::max(Sigma[1], Sigma[2])) / k_r;

		for (size_t k = 0; k < Sigma.size(); k++)
			Sigma[k] = std::max(Sigma[k], MINIMUM);

		Sigma *= k_s;
	}

	// ??? leave out first R because of eq. 10 in [000]
	// CANNOT BE LEFT OUT RIGHT NOW
	Eigen::Matrix3f G_Eigen =
		m_Dataset->ParticleRadiusInv * R * Sigma.cwiseInverse().asDiagonal() * R.transpose();

	G = {
		eigenToGlm(G_Eigen.col(0)),
		eigenToGlm(G_Eigen.col(1)),
		eigenToGlm(G_Eigen.col(2)),
	};
}

void RayMarcher::PerPixel_Isotropic(uint32_t index, void* _locals)
{
	ThreadLocals* locals = reinterpret_cast<ThreadLocals*>(_locals);

	const float z = m_Depth[index];

	m_Positions[index] = glm::vec4(0);
	m_Normals[index] = glm::vec4(0);
	if (z == 1.0f) return;

	auto& frame = m_Dataset->Frames[m_Settings.Frame];

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

		// check density grid
		OctreeNode* gridNode = nullptr;

		while ((gridNode = frame.QueryDensityGrid(position)) && gridNode->Flag == false)
		{
			if (!(
				position.x >= gridNode->Min.x &&
				position.x <= gridNode->Max.x &&
				position.y >= gridNode->Min.y &&
				position.y <= gridNode->Max.y &&
				position.z >= gridNode->Min.z &&
				position.z <= gridNode->Max.z
				))
			{
				SPDLOG_ERROR("{} {} {}\n{} {} {}\n{} {} {}",
							 position.x, position.y, position.z,
							 gridNode->Min.x, gridNode->Min.y, gridNode->Min.z,
							 gridNode->Max.x, gridNode->Max.y, gridNode->Max.z);
			}

			// skip empty space
			position = intersectAABB(position, step, gridNode->Min, gridNode->Max) + step;
			//position += step;

			//SPDLOG_WARN("skip");
		}

		// find neighbors
		const std::vector<uint32_t> neighbors =
			m_Dataset->GetNeighbors(position, m_Settings.Frame);

		locals->NumNeighbors = std::min(neighbors.size(), size_t(MAX_NEIGHBORS));

		// store neighbor positions
		for (uint32_t i = 0; i < locals->NumNeighbors; i++)
		{
			const glm::vec3 r = frame.m_Particles[neighbors[i]] - position;
			locals->NeighborPositions_Rel[i] = r;
		}

		// density
		float density = 0.0f;

		for (uint32_t i = 0; i < locals->NumNeighbors; i++)
			density += m_IsotropicKernel.W(locals->NeighborPositions_Rel[i]);

		if (density >= m_Settings.IsoDensity)
		{
			// set position
			m_Positions[index] = glm::vec4(position, 1);

			// compute object normal
			glm::vec3 normal(0);

			for (uint32_t i = 0; i < locals->NumNeighbors; i++)
				normal += m_IsotropicKernel.gradW(locals->NeighborPositions_Rel[i]);

			m_Normals[index] = glm::vec4(glm::normalize(normal), 1);

			// break
			i = m_Settings.MaxSteps;
		}
	}
}

void RayMarcher::PerPixel_Anisotropic(uint32_t index, void* _locals)
{
	ThreadLocals* locals = reinterpret_cast<ThreadLocals*>(_locals);

	const float z = m_Depth[index];

	m_Positions[index] = glm::vec4(0);
	m_Normals[index] = glm::vec4(0);
	if (z == 1.0f) return;

	auto& frame = m_Dataset->Frames[m_Settings.Frame];

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

#if 1
		// check density grid
		OctreeNode* gridNode = nullptr;

		// skip empty space
		while ((gridNode = frame.QueryDensityGrid(position)) && gridNode->Flag == false)
			position = intersectAABB(position, step, gridNode->Min, gridNode->Max) + step;
#endif

		const std::vector<unsigned int> neighbors =
			m_Dataset->GetNeighborsExt(position, m_Settings.Frame);
		locals->NumNeighborsExt = std::min(neighbors.size(), size_t(MAX_NEIGHBORS));
		locals->NumNeighbors = 0;

		// store neighbor positions
		for (uint32_t i = 0; i < locals->NumNeighborsExt; i++)
		{
			locals->NeighborPositions_AbsExt[i] = frame.m_ParticlesExt[neighbors[i]];

			const glm::vec3 r = locals->NeighborPositions_AbsExt[i] - position;

			if (glm::dot(r, r) < m_Dataset->ParticleRadius * m_Dataset->ParticleRadius)
				locals->NeighborPositions_Rel[locals->NumNeighbors++] = r;
		}

		// compute G and density
		glm::mat3 G;
		float density = 0.0f;

		WPCA(position, locals->NeighborPositions_AbsExt, locals->NumNeighborsExt, G);
		const float detG = glm::determinant(G);

		for (uint32_t i = 0; i < locals->NumNeighbors; i++)
			density += m_AnisotropicKernel.W(G, detG, locals->NeighborPositions_Rel[i]);

		if (density >= m_Settings.IsoDensity)
		{
			// set position
			m_Positions[index] = glm::vec4(position, 1);

			// compute object normal
			glm::vec3 normal(0);

			for (uint32_t i = 0; i < locals->NumNeighbors; i++)
				normal += m_AnisotropicKernel.gradW(G, detG, locals->NeighborPositions_Rel[i]);

			m_Normals[index] = glm::vec4(glm::normalize(normal), 1);

			// break
			i = m_Settings.MaxSteps;
		}
	}
}
