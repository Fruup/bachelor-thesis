#pragma once

#include <Partio.h>
#include <CompactNSearch.h>

#include <engine/assets/AssetPath.h>

//using Particle = std::array<float, 3>;
using Particle = glm::vec3;

class Dataset
{
	using Frame = std::vector<Particle>;

public:
	Dataset(const std::string& pathPrefix,
			const std::string& pathSuffix = ".bgeo",
			int startIndex = 1);

	// generates a fluid block by randomly sampling a cube
	Dataset(float extent, size_t numParticles);

	~Dataset();

	std::vector<uint32_t> GetNeighbors(const glm::vec3& position, uint32_t index);
	std::vector<uint32_t> GetNeighborsExt(const glm::vec3& position, uint32_t index);

	float ParticleRadius;
	float ParticleRadiusExt;
	float ParticleRadiusInv;
	float ParticleRadiusExtInv;

	std::vector<Frame> Frames;
	std::vector<Frame> FramesExt; // a copy of 'Frames' for extended n-search
	std::vector<CompactNSearch::NeighborhoodSearch> NSearch;
	std::vector<CompactNSearch::NeighborhoodSearch> NSearchExt;

	size_t MaxParticles = 0;
	bool Loaded = false;

private:
	void ReadFile(Partio::ParticlesDataMutable* file);
	void BuildCompactNSearch();
};
