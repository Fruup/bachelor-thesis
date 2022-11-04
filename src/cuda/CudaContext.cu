#include "CudaContext.cuh"

// --------------------------------------------------------------

CudaContext& CudaContext::Instance()
{
    static CudaContext ctx;
    return ctx;
}

// --------------------------------------------------------------

bool CudaContext::Init(const vk::Device& vkdevice, uint8_t* vkDeviceUUID, size_t uuidSize)
{
    m_Device = vkdevice;

    // Set the same device that Vulkan uses
    int device = FindDevice(vkDeviceUUID, uuidSize);
    if (device < 0)
    {
        //SPDLOG_ERROR("Failed to find the Cuda device that Vulkan uses.");
        return false;
    }

    checkCudaErrors(cudaSetDevice(device));

    // create stream
    checkCudaErrors(cudaStreamCreateWithFlags(&m_Stream, cudaStreamNonBlocking));

    return true;
}

CudaImageMemoryImportResult CudaContext::ImportImageMemory(const ExternalImage& extImage)
{
    // create CUDA resource handle
    cudaExternalMemoryHandleDesc extMemHandleDesc;
    memset(&extMemHandleDesc, 0, sizeof(extMemHandleDesc));

    extMemHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
    extMemHandleDesc.handle.win32.handle = extImage.GetMemoryHandle();
    extMemHandleDesc.size = extImage.m_MemorySize;

    cudaExternalMemory_t extMem;
    checkCudaErrors(cudaImportExternalMemory(&extMem, &extMemHandleDesc));

    // map mipmap levels to CUDA array layers
    cudaExternalMemoryMipmappedArrayDesc extMemMipmappedArrayDesc;
    memset(&extMemMipmappedArrayDesc, 0, sizeof(extMemMipmappedArrayDesc));

    cudaExtent extent{ extImage.m_Width, extImage.m_Height, 0 /* depth=0 for 2D images */};

    cudaChannelFormatDesc formatDesc;
    formatDesc.x = extImage.m_NumChannels > 0 ? extImage.m_BitsPerChannel : 0;
    formatDesc.y = extImage.m_NumChannels > 1 ? extImage.m_BitsPerChannel : 0;
    formatDesc.z = extImage.m_NumChannels > 2 ? extImage.m_BitsPerChannel : 0;
    formatDesc.w = extImage.m_NumChannels > 3 ? extImage.m_BitsPerChannel : 0;
    formatDesc.f = cudaChannelFormatKindFloat;

    extMemMipmappedArrayDesc.offset = 0;
    extMemMipmappedArrayDesc.formatDesc = formatDesc;
    extMemMipmappedArrayDesc.extent = extent;
    extMemMipmappedArrayDesc.numLevels = 1;
    extMemMipmappedArrayDesc.flags =
#if 1
        //0
        cudaArraySurfaceLoadStore
#else
        cudaArraySurfaceLoadStore |
        (
            extImage.m_Usage & vk::ImageUsageFlagBits::eColorAttachment
            ? cudaArrayColorAttachment
            : 0
        )
#endif
        ;

    cudaMipmappedArray_t mipmappedArray;
    checkCudaErrors(cudaExternalMemoryGetMappedMipmappedArray(
        &mipmappedArray, extMem, &extMemMipmappedArrayDesc));

    checkCudaErrors(cudaMallocMipmappedArray(&mipmappedArray, &formatDesc, extent, 1));

    // for each mipmap level (we use only one)
    cudaArray_t mipLevelArray; // the data array for one mip level

    checkCudaErrors(cudaGetMipmappedArrayLevel(&mipLevelArray, mipmappedArray, 0 /* level */));

    // create surface
    cudaResourceDesc resourceDesc;
    memset(&resourceDesc, 0, sizeof(resourceDesc));
    resourceDesc.resType = cudaResourceTypeArray;
    resourceDesc.res.array.array = mipLevelArray;

    cudaSurfaceObject_t surfaceObject;
    checkCudaErrors(cudaCreateSurfaceObject(&surfaceObject, &resourceDesc));

#if 0
    // create mipmapped array resource
    memset(&resourceDesc, 0, sizeof(resourceDesc));

    resourceDesc.resType = cudaResourceTypeMipmappedArray;
    resourceDesc.res.mipmap.mipmap = mipmappedArray;

    // create texture
    cudaTextureDesc textureDesc;
    memset(&textureDesc, 0, sizeof(textureDesc));

    // maybe can be false?
    textureDesc.normalizedCoords = true;
    textureDesc.filterMode = cudaFilterModePoint;
    textureDesc.mipmapFilterMode = cudaFilterModePoint;

    textureDesc.addressMode[0] = cudaAddressModeWrap;
    textureDesc.addressMode[1] = cudaAddressModeWrap;

    textureDesc.maxMipmapLevelClamp = 0.0f;
    textureDesc.readMode = cudaReadModeElementType;

    cudaTextureObject_t textureObject;
    checkCudaErrors(cudaCreateTextureObject(&textureObject, &resourceDesc,
                                            &textureDesc, NULL));
#endif

#if 0
    static std::vector<float> _d(1600*200*4);
    for (size_t i = 0; i < std::size(_d); i++)
        _d[i] = 1.0f;

    checkCudaErrors(
        cudaMemcpyToArray(mipLevelArray, 0, 0, _d.data(), sizeof(float) * _d.size(), cudaMemcpyHostToDevice));
#endif

    // allocate space for surface handles on device
    cudaSurfaceObject_t* deviceSurfaceObject;

    checkCudaErrors(cudaMalloc((void**)&deviceSurfaceObject, sizeof(cudaSurfaceObject_t)));
    checkCudaErrors(cudaMemcpy(deviceSurfaceObject,
                               &surfaceObject,
                               sizeof(cudaSurfaceObject_t),
                               cudaMemcpyHostToDevice));

    // return
    CudaImageMemoryImportResult result;
    result.surfaceObject = surfaceObject;
    result.deviceSurfaceObject = deviceSurfaceObject;
    result.externalMemory = extMem;
    //result.textureObject = textureObject;

    return result;
}

void CudaContext::ImportSemaphores(HANDLE semaphoreVK, HANDLE semaphoreCU)
{
    ImportSemaphore(semaphoreVK, &m_ExternalSemaphoreVK);
    ImportSemaphore(semaphoreCU, &m_ExternalSemaphoreCU);
}

void CudaContext::WaitForVulkan()
{
    // wait for m_ExternalSemaphoreVK
    cudaExternalSemaphoreWaitParams params;
    memset(&params, 0, sizeof(params));

    params.params.fence.value = 0;
    params.flags = 0;

    checkCudaErrors(
        cudaWaitExternalSemaphoresAsync(&m_ExternalSemaphoreVK, &params, 1, m_Stream));
}

void CudaContext::SignalVulkan()
{
    // signal m_ExternalSemaphoreCU
    cudaExternalSemaphoreSignalParams params;
    memset(&params, 0, sizeof(params));

    params.params.fence.value = 0;
    params.flags = 0;

    checkCudaErrors(
        cudaSignalExternalSemaphoresAsync(&m_ExternalSemaphoreCU, &params, 1, m_Stream));
}

int CudaContext::FindDevice(uint8_t* vkDeviceUUID, size_t uuidSize)
{
    int current_device = 0;
    int device_count = 0;
    int devices_prohibited = 0;

    cudaDeviceProp deviceProp;
    checkCudaErrors(cudaGetDeviceCount(&device_count));

    if (device_count == 0) {
        fprintf(stderr, "CUDA error: no devices supporting CUDA.\n");
        exit(EXIT_FAILURE);
    }

    // Find the GPU which is selected by Vulkan
    while (current_device < device_count) {
        cudaGetDeviceProperties(&deviceProp, current_device);

        if ((deviceProp.computeMode != cudaComputeModeProhibited)) {
            // Compare the cuda device UUID with vulkan UUID
            int ret = memcmp((void *)&deviceProp.uuid, vkDeviceUUID, uuidSize);
            if (ret == 0) {
                checkCudaErrors(cudaSetDevice(current_device));
                checkCudaErrors(cudaGetDeviceProperties(&deviceProp, current_device));
                printf("GPU Device %d: \"%s\" with compute capability %d.%d\n\n",
                       current_device, deviceProp.name, deviceProp.major,
                       deviceProp.minor);

                return current_device;
            }

        } else {
            devices_prohibited++;
        }

        current_device++;
    }

    if (devices_prohibited == device_count) {
        fprintf(stderr,
                "CUDA error:"
                " No Vulkan-CUDA Interop capable GPU found.\n");
        exit(EXIT_FAILURE);
    }

    return -1;
}

void CudaContext::ImportSemaphore(HANDLE handle,
                                  cudaExternalSemaphore_t* out)
{
    cudaExternalSemaphoreHandleDesc externalSemaphoreHandleDesc;
    memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));

    externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeOpaqueWin32;
    externalSemaphoreHandleDesc.handle.win32.handle = handle;

    externalSemaphoreHandleDesc.flags = 0;

    checkCudaErrors(cudaImportExternalSemaphore(out, &externalSemaphoreHandleDesc));
}
