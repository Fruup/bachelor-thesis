#include "engine/hzpch.h"
#include "AssetPath.h"
#include "AssetManager.h"

AssetPathAbs::AssetPathAbs(const AssetPathRel& relativePath, bool useEngineDir)
{
	m_Path =
		useEngineDir ?
			AssetManager::Data.EngineAssetsDirectory / relativePath :
			AssetManager::Data.AssetsDirectory / relativePath;
}

bool AssetPathAbs::Exists() const
{
	return std::filesystem::exists(m_Path);
}

// --------------------------------------------------------------------------------

AssetPathAbs AssetPathRel::MakeAbsolute(bool useEngineDir) const
{
	return { *this, useEngineDir };
}

AssetPathRel AssetPathRel::ApplyFixes(const std::string& filenamePrefix, const std::string& filenameSuffix) const
{
	return m_Path.parent_path() / (filenamePrefix + m_Path.filename().string() + filenameSuffix);
}

AssetPathRel AssetPathRel::ChooseSuffix(const std::vector<const char*>& suffixes) const
{
	for (auto& suffix : suffixes)
	{
		auto p = operator + (suffix);

		if (p.MakeAbsolute().Exists())
			return p;
	}

	HZ_ASSERT(false, "Whoops");
	return *this;
}

AssetPathRel AssetPathRel::operator / (const std::string& suffix) const
{
	HZ_ASSERT(!m_Path.has_extension(), "This path cannot have a filename!");
	return m_Path / suffix;
}

AssetPathRel AssetPathRel::operator + (const std::string& suffix) const
{
	return ApplyFixes("", suffix);
}

inline void AssetPathRel::AssertRelativity()
{
	HZ_ASSERT(
		m_Path.string().find("assets") == std::string::npos &&
		!m_Path.has_root_directory(),
		"This path should be relative to the assets directory!"
	);
}
