#pragma once

class AssetPathRel;

class AssetPathAbs
{
public:
	AssetPathAbs(const AssetPathRel& relativePath, bool useEngineDir = false);

	bool Exists() const;

	const std::filesystem::path& path() const { return m_Path; }
	std::string string() const { return m_Path.string(); }
	operator std::string() const { return m_Path.string(); }

private:
	std::filesystem::path m_Path;
};

class AssetPathRel
{
public:
	AssetPathRel() = default;

	template <typename T>
	AssetPathRel(const T& path)
	{
		m_Path = std::filesystem::path(path);
		AssertRelativity();
	}

public:
	AssetPathAbs MakeAbsolute(bool useEngineDir = false) const;
	AssetPathRel MakeTemp() const { return ".temp" / m_Path; }
	AssetPathRel ApplyFixes(const std::string& filenamePrefix, const std::string& filenameSuffix) const;

	AssetPathRel ChooseSuffix(const std::vector<const char*>& suffixes) const; // choose suffix for which the file exists

	bool IsEngineFile() const
	{
		auto abs = MakeAbsolute(true);
		return abs.Exists();
	}

	AssetPathRel operator / (const std::string& suffix) const;
	AssetPathRel operator + (const std::string& suffix) const;

	operator const std::filesystem::path& () const { return m_Path; }

private:
	void AssertRelativity();

private:
	std::filesystem::path m_Path;

	friend struct fmt::formatter<AssetPathRel>;
};

template <>
struct fmt::formatter<AssetPathRel>
{
	constexpr auto parse(format_parse_context& ctx)
	{
		return ctx.end();
	}

	template <typename Context>
	auto format(const AssetPathRel& p, Context& ctx)
	{
		return fmt::format_to(ctx.out(), "{}", p.m_Path.string().c_str());
	}
};
