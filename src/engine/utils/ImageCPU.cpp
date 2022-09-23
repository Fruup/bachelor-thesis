#include "engine/hzpch.h"
#include "engine/utils/ImageCPU.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

ImageCPU::ImageCPU(const std::string& filename)
{
	static_assert(sizeof(uint8_t) == sizeof(stbi_uc));

	int width, height;
	uint8_t* loadedData = stbi_load(filename.c_str(), &width, &height, &NumChannels, 4);
	if (!loadedData)
		SPDLOG_ERROR("Failed to load image '{}'!", filename);
	else
	{
		bool ownData = false;
		uint8_t* data = loadedData;

#if 0
		if (NumChannels == 3)
		{
			size_t N = size_t(width * height);
			data = new uint8_t[4 * N];

			for (size_t i = 0; i < N; ++i)
			{
				memcpy(data + (4 * i), loadedData + (3 * i), 3);
				data[4 * i + 3] = 255;
			}

			stbi_image_free(loadedData);
			loadedData = nullptr;

			NumChannels = 4;
			ownData = true;
		}
#endif

		Width = uint32_t(width);
		Height = uint32_t(height);
		Size = Width * Height;
		NumBytes = Size * uint32_t(NumChannels);

		Data.resize(NumBytes);
		memcpy(Data.data(), data, NumBytes);

		if (!ownData) stbi_image_free(data);
		else delete[] data;
	}
}

ImageCPU::ImageCPU(uint32_t width, uint32_t height) :
	Width(width), Height(height), Size(width * height), NumBytes(4 * width * height), NumChannels(4)
{
	Data.resize(Size);
}

void ImageCPU::Blit(ImageCPU& target, vk::Rect2D sourceRect, vk::Offset2D targetOffset)
{
	for (uint32_t y = 0; y < sourceRect.extent.height; y++)
		memcpy(
			target.Data.data() + ((targetOffset.y + y) * target.Width + targetOffset.x),
				   Data.data() + ((sourceRect.offset.y + y) * Width + sourceRect.offset.x),
			sizeof(uint32_t) * sourceRect.extent.width
		);
}
