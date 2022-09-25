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

	void PrepareGPULayoutForRendering(vk::CommandBuffer& cmd);
	void PrepareGPULayoutForCopying(vk::CommandBuffer& cmd);

	void CopyToGPU();
	void CopyFromGPU();

	void* MapCPUMemory(vk::DeviceSize offset = 0, vk::DeviceSize size = 0);
	void UnmapCPUMemory();

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

		vk::ImageLayout Layout;
	} GPU;

	_Type Type;

	vk::ImageAspectFlags AspectFlags;
	vk::Format Format;
	vk::ImageUsageFlags Usage;
	vk::DeviceSize Size;
};
