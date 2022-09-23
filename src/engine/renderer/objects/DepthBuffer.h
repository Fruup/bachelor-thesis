#pragma once

class DepthBuffer
{
public:
	bool Create();
	void Destroy();

	operator bool() const { return m_Image && m_View && m_Memory; }

	const vk::Image& GetImage() const { return m_Image; }
	const vk::ImageView& GetView() const { return m_View; }
	const vk::DeviceMemory& GetMemory() const { return m_Memory; }

private:
	vk::Image m_Image;
	vk::ImageView m_View;
	vk::DeviceMemory m_Memory;
};
