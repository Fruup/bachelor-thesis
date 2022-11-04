#pragma once

#include "app/Kernel.h"

#include <engine/camera/Camera3D.h>
#include <engine/camera/CameraController3D.h>

#include "app/ThreadPool.h"
#include "app/VisualizationSettings.h"

class Dataset;

class RayMarcher
{
public:
	RayMarcher();

	void Exit();

	void Prepare(const VisualizationSettings& settings,
				 const CameraController3D& camera,
				 Dataset* dataset,
				 glm::vec4* positions,
				 glm::vec4* normals,
				 float* depth);

	void Start();

	bool IsDone() { return m_ThreadPool.IsDone(); }

private:
	void WPCA(const glm::vec3& particle,
			  const glm::vec3* neighborPositions,
			  uint32_t N,
			  glm::mat3& G,
			  bool verbose = false);

	void PerPixel_Isotropic(uint32_t index, void* locals);
	void PerPixel_Anisotropic(uint32_t index, void* locals);

private:
	VisualizationSettings m_Settings;
	Dataset* m_Dataset;

	uint32_t m_Width;
	uint32_t m_Height;
	float m_TwoWidthInv;
	float m_TwoHeightInv;

	glm::mat4 m_InvProjectionView;
	glm::vec3 m_CameraPosition;

	glm::vec4* m_Positions;
	glm::vec4* m_Normals;
	float* m_Depth;

	CubicSplineKernel m_IsotropicKernel;
	AnisotropicKernel m_AnisotropicKernel;

	//BS::thread_pool m_ThreadPool;
	ThreadPool m_ThreadPool;
};
