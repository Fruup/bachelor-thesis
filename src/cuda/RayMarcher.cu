#include "RayMarcher.cuh"

#include "CudaContext.cuh"

#include <surface_indirect_functions.h>

// -------------------------------------------------------------------------
// KERNEL

__global__ void ray_marching(VisualizationSettings settings,
							 cudaSurfaceObject_t depth,
							 cudaSurfaceObject_t depthAgg,
							 cudaSurfaceObject_t* positions,
							 cudaSurfaceObject_t normals,
							 size_t width, size_t height)
{
	//surf2Dread<float>(depth, x, y);

	for (size_t i = 50 * 1600; i < 52 * 1600; i++)
		surf2Dwrite<float>(1.0f, positions[0], 4 * (i % width), i / width);

	return;

	const size_t stride = gridDim.x * blockDim.x;

	for (size_t tid = blockIdx.x * blockDim.x + threadIdx.x; tid < width * height; tid += stride)
	{
		const int x = tid % width;
		const int y = tid / width;

		// byte-addressed, so multiply x coordinate with 4 * sizeof(float)
		//float val = surf2Dread<float>(depth, sizeof(float) * x, y);

		//surf2Dwrite<float4>(make_float4(val, val, val, val), positions, sizeof(float4) * x, y);

		/*surf2Dwrite<float4>(make_float4(1.0f, 1.0f, 1.0f, 1.0f), positions, 16 * x, y);
		surf2Dwrite<float4>(make_float4(1.0f, 1.0f, 1.0f, 1.0f), normals, 16 * x, y);*/

		//const float z = depth[tid];

		/*positions[tid] = glm::vec4(0);
		normals[tid] = glm::vec4(0);
		if (z == 1.0f) continue;*/

#if 0
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

#if 0
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

#endif
	}
}

// -------------------------------------------------------------------------

void CudaRayMarcher::Setup(const VisualizationSettings& settings,
						   const cudaSurfaceObject_t& depth,
						   const cudaSurfaceObject_t& depthAgg,
						   cudaSurfaceObject_t* positions,
						   const cudaSurfaceObject_t& normals,
						   uint32_t width,
						   uint32_t height)
{
	m_Settings = settings;

	m_Depth = depth;
	m_DepthAgg = depthAgg;
	m_Positions = positions;
	m_Normals = normals;
	m_Width = width;
	m_Height = height;

	m_NumThreadsPerBlock = 32;
	m_NumBlocks = (width * height + m_NumThreadsPerBlock - 1) / m_NumThreadsPerBlock;
}

void CudaRayMarcher::Run()
{
	checkCudaErrors(cudaPeekAtLastError());

	// lauch kernel
	ray_marching<<<m_NumBlocks, m_NumThreadsPerBlock, 0, Cuda.m_Stream>>>(
		m_Settings,
		m_Depth,
		m_DepthAgg,
		m_Positions,
		m_Normals,
		m_Width,
		m_Height);

	checkCudaErrors(cudaDeviceSynchronize());
	//checkCudaErrors(cudaStreamSynchronize(Cuda.m_Stream));
	checkCudaErrors(cudaPeekAtLastError());
}
