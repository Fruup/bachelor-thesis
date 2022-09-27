#pragma once

/**
* Buffer used for per-pixel geometry information like positions or depth.
* Can be used as a framebuffer attachment.
* Can be copied between host and device.
*/
class BilateralBuffer
{
public:
	enum _Type
	{
		Depth,
		Color,
	};

public:
	void Init(_Type type);
	void Exit();

	void CopyToGPU();
	void CopyFromGPU();

	void* MapCPUMemory(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);
	void UnmapCPUMemory();

	void TransitionLayout(vk::ImageLayout newLayout,
						  vk::AccessFlags accessMask,
						  vk::PipelineStageFlags stage);

private:
	void CreateCPUSide();
	void CreateGPUSide();

public:
	struct
	{
		vk::Buffer Buffer;
		vk::DeviceMemory Memory;
	} CPU;

	struct
	{
		vk::Image Image;
		vk::ImageView ImageView;
		vk::DeviceMemory Memory;
		vk::Sampler Sampler;

		vk::ImageLayout Layout;
	} GPU;

	_Type Type;

	vk::ImageAspectFlags AspectFlags;
	vk::Format Format;
	vk::ImageUsageFlags Usage;
	vk::DeviceSize Size;
};
