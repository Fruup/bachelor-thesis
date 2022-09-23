#include "engine/hzpch.h"
#include "engine/assets/AssetManager.h"
#include "engine/utils/Utils.h"
#include "engine/assets/AssetPath.h"

AssetManagerData AssetManager::Data;

void AssetManager::Init()
{
	// find assets directories
	Data.AssetsDirectory = "assets";

#if PRODUCTION
	Data.EngineAssetsDirectory = "assets";
#else
	Data.EngineAssetsDirectory = "../../assets";
#endif

	Data.TempDirectory = Data.AssetsDirectory / ".temp";

	std::filesystem::create_directories(Data.AssetsDirectory);
	std::filesystem::create_directories(Data.TempDirectory);

	Data.Sprites.reserve(32);
}

void AssetManager::Exit()
{
	Data.Sprites.clear();
}

void AssetManager::LoadStartupAssets()
{
}

std::string AssetManager::GetAssetPath(const std::filesystem::path& relativePath)
{
	if (Data.AssetsDirectory != "")
	{
		std::filesystem::path p = Data.AssetsDirectory / relativePath;
		if (std::filesystem::exists(p))
			return p.string();
	}
	
#if DEBUG || RELEASE
	if (Data.EngineAssetsDirectory != "")
	{
		std::filesystem::path p = Data.EngineAssetsDirectory / relativePath;
		if (std::filesystem::exists(p))
			return p.string();
	}
#endif

	SPDLOG_WARN("Asset not found in either application or engine asset directory! ({})", relativePath.string().c_str());
	return "";
}

AssetHandleBase AssetManager::LoadSprite(const AssetPathRel& relativePath, const char* name)
{
	// load yml file
	auto ymlPath = (relativePath + ".yml").MakeAbsolute();

	YAML::Node yml;
	if (ymlPath.Exists())
		yml = YAML::LoadFile(ymlPath);

	// add sprite
	AssetHandleBase handle = name ? name : AssetHandleBase::GetFresh();
	Assets::Sprite& sprite = Data.Sprites[handle];

	// load image
	{
		AssetPathRel imgPath;

		if (yml && yml["texture"])
			imgPath = relativePath / yml["texture"].as<std::string>();
		else
			imgPath = relativePath.ChooseSuffix({ ".png", ".jpg", ".bmp" });

		AssetPathAbs imgPathAbs = imgPath.MakeAbsolute();
		HZ_ASSERT(imgPathAbs.Exists(), "Image path does not exist!");

		sprite.Image.Create(imgPathAbs);
	}

	// load normal
	{
		AssetPathRel normalPath;

		if (yml && yml["normal"])
			normalPath = yml["normal"].as<std::string>();
		else
			normalPath = relativePath + "-normal.png";

		AssetPathAbs normalPathAbs = normalPath.MakeAbsolute();

		if (normalPathAbs.Exists())
		{
			sprite.NormalMap.Create(
				normalPathAbs,
				ImageCreateOptions{
					.sRGB = false,
					.Filter = vk::Filter::eLinear,
				});
		}
	}

	// add frames
	if (yml && yml["frames"])
	{
		for (const auto& frame : yml["frames"])
		{
			sprite.AddFrame(
				frame["frame"]["x"].as<float>(),
				frame["frame"]["y"].as<float>(),
				frame["frame"]["w"].as<float>(),
				frame["frame"]["h"].as<float>(),
				frame["duration"].as<float>() * 1e-3f
			);
		}
	}

	return handle;
}
