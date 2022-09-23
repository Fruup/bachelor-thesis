#pragma once

#include <engine/assets/Sprite.h>
#include <engine/utils/Utils.h>

using AssetIdentifier = uint32_t;

struct AssetHandleBase
{
	AssetHandleBase() = default;

	constexpr AssetHandleBase(AssetIdentifier id) : Id(id) {}
	constexpr AssetHandleBase(const char* name) :
		AssetHandleBase(AssetIdentifier(Utils::HashString(name))) {}

	AssetIdentifier Id{};
	size_t Hash{};

	operator AssetIdentifier() const { return Id; }
	operator bool() const { return Id; }

	static AssetHandleBase GetFresh();
};

template <typename T>
struct AssetHandle : public AssetHandleBase
{
	AssetHandle() = default;
	AssetHandle(AssetHandleBase handle);
	constexpr AssetHandle(const char* handle) : AssetHandle(AssetHandleBase(handle)) {}
	constexpr AssetHandle(AssetIdentifier handle) : AssetHandle(AssetHandleBase(handle)) {}

	operator bool() const { return Asset && AssetHandleBase::operator bool(); }

	operator T& ()
	{
		HZ_ASSERT(bool(this), "Handle is invalid!");
		return *Asset;
	}

	operator const T& () const
	{
		HZ_ASSERT(bool(this), "Handle is invalid!");
		return *Asset;
	}

	T* Asset{ nullptr };
};

template <>
AssetHandle<Assets::Sprite>::AssetHandle(AssetHandleBase handle);
