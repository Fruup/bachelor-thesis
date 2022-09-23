#include <engine/hzpch.h>
#include <engine/assets/AssetHandle.h>

#include <engine/assets/AssetManager.h>

template <>
AssetHandle<Assets::Sprite>::AssetHandle(AssetHandleBase handle) :
	AssetHandleBase(handle)
{
	Asset = &AssetManager::GetSprite(handle);
}

AssetHandleBase AssetHandleBase::GetFresh()
{
	static AssetIdentifier s_NextId = 1;
	return s_NextId++;
}
