#pragma once

#include <cuda_runtime_api.h>

#include <Windows.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_win32.h>

#include "app/ExternalImage.h"

#include "CudaHelpers.cuh"

#include <spdlog/spdlog.h>
#include <engine/Engine.h>

#undef checkCudaErrors
#define checkCudaErrors(r) \
	HZ_ASSERT(r == 0, "CUDA ERROR (code: {}, name: {}): {}", \
		      uint32_t(r), \
			  cudaGetErrorName(r), \
			  cudaGetErrorString(r))

// ----------------------------------------------------------------

struct CudaImageMemoryImportResult
{
	cudaSurfaceObject_t surfaceObject;
	cudaSurfaceObject_t* deviceSurfaceObject; // device memory address of the surface object
	cudaExternalMemory_t externalMemory;
	//cudaTextureObject_t textureObject;
};

class CudaContext
{
public:
	bool Init(const vk::Device& device, uint8_t* vkDeviceUUID, size_t uuidSize);
	//void Exit();

	CudaImageMemoryImportResult ImportImageMemory(const ExternalImage& extImage);

	void ImportSemaphores(HANDLE semaphoreVK, HANDLE semaphoreCU);

	void WaitForVulkan();
	void SignalVulkan();

	static CudaContext& Instance();

	//RayMarcher m_RayMarcher;

	cudaStream_t m_Stream;

private:
	int FindDevice(uint8_t* vkDeviceUUID, size_t uuidSize);

	void ImportSemaphore(HANDLE semaphoreHandle, cudaExternalSemaphore_t* out);

private:
	vk::Device m_Device;

	int m_CudaDevice;

	cudaExternalSemaphore_t m_ExternalSemaphoreVK;
	cudaExternalSemaphore_t m_ExternalSemaphoreCU;
};

#define Cuda CudaContext::Instance()
