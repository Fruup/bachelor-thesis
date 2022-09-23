#pragma once

class ImageCPU
{
public:
	ImageCPU() = default;
	ImageCPU(const ImageCPU& rhs) = default;
	ImageCPU(const std::string& filename);
	ImageCPU(uint32_t width, uint32_t height);

	void Blit(ImageCPU& target, vk::Rect2D sourceRect, vk::Offset2D targetOffset);

	operator uint8_t* () { return Data.data(); }

	uint32_t Width{}, Height{};
	uint32_t Size{}, NumBytes{};
	std::vector<uint8_t> Data;
	int NumChannels{};
};
