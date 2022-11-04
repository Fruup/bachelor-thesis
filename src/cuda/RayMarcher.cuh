#pragma once

#include <cuda_runtime_api.h>

#include "app/VisualizationSettings.h"

#include <stdint.h>

class CudaRayMarcher
{
public:
	void Setup(const VisualizationSettings& settings,
			   const cudaSurfaceObject_t& depth,
			   const cudaSurfaceObject_t& depthAgg,
			   cudaSurfaceObject_t* positions,
			   const cudaSurfaceObject_t& normals,
			   uint32_t width,
			   uint32_t height);

	void Run();

private:
	VisualizationSettings m_Settings;

	cudaSurfaceObject_t m_Depth;
	cudaSurfaceObject_t m_DepthAgg;
	cudaSurfaceObject_t* m_Positions;
	cudaSurfaceObject_t m_Normals;

	uint32_t m_Width, m_Height;
	size_t m_NumBlocks, m_NumThreadsPerBlock;
};
