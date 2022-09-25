#pragma once

#include "engine/utils/Utils.h"
#include "engine/assets/AssetPath.h"

struct AssetManagerData
{
	std::filesystem::path EngineAssetsDirectory;
	std::filesystem::path AssetsDirectory;
	std::filesystem::path TempDirectory;
};

class AssetManager
{
public:
	static void Init();
	static void LoadStartupAssets();
	static void Exit();

	inline static const std::filesystem::path& GetAssetDirectory() { return Data.AssetsDirectory; }
	inline static const std::filesystem::path& GetTempDirectory() { return Data.TempDirectory; }

	static std::filesystem::path MakeEngineAssetPath(const std::filesystem::path& relativePath)
	{
		return Data.EngineAssetsDirectory / relativePath;
	}

	static std::filesystem::path MakeAssetPath(const std::filesystem::path& relativePath)
	{
		return Data.AssetsDirectory / relativePath;
	}

	static std::string GetAssetPath(const std::filesystem::path& relativePath);

	static AssetManagerData Data;
};
