#pragma once

#include <Partio.h>
#include <CompactNSearch.h>

#include <engine/assets/AssetPath.h>

using Particle = std::array<float, 3>;

class Dataset
{
	using Snapshot = std::vector<Particle>;

public:
	Dataset(
		const std::string& pathPrefix,
		const std::string& pathSuffix = ".bgeo",
		int startIndex = 1);

	~Dataset();

	std::vector<uint32_t> GetNeighbors(const glm::vec3& position, uint32_t index);

	const float ParticleRadius;

	std::vector<Snapshot> Snapshots;
	std::vector<CompactNSearch::NeighborhoodSearch> NSearch;

	size_t MaxParticles = 0;
	bool Loaded = false;

private:
	void ReadFile(Partio::ParticlesDataMutable* file);
	void BuildCompactNSearch();
};
