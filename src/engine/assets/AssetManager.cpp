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
}

void AssetManager::Exit()
{
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
