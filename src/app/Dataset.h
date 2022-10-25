#pragma once

#include <Partio.h>
#include <CompactNSearch.h>

#include <engine/assets/AssetPath.h>

#include "app/Kernel.h"

class Dataset;

using Particle = glm::vec3;

struct OctreeNode
{
	std::vector<uint32_t> ParticleIndices;
	uint32_t NumParticles;
	bool Flag;

	OctreeNode* Children[8] = {};

	glm::vec3 Min, Max;

	bool IsLeaf() const { return !Children[0]; }
};

class DensityGrid
{
public:
	std::vector<OctreeNode> m_Nodes;

	float m_CellWidth;
	glm::vec3 m_InvCellWidthVec;
	uint32_t m_Width, m_Height, m_Depth;
};

class Frame
{
public:
	Frame() = default;
	Frame(const Frame&) = default;

	Frame(Dataset* dataset,
		  const std::vector<Particle>& particles,
		  float neighborhoodRadius,
		  float neighborhoodRadiusExt);

	OctreeNode* QueryDensityGrid(const glm::vec3& x);

	void BuildSearch();
	void ComputeAABB();
	void BuildDensityGrid(float isoDensity);

	std::vector<Particle> m_Particles;
	CompactNSearch::NeighborhoodSearch m_Search;
	
	std::vector<Particle> m_ParticlesExt;
	CompactNSearch::NeighborhoodSearch m_SearchExt;

	glm::vec3 m_Min;
	glm::vec3 m_Max;
	DensityGrid m_DensityGrid;

	Dataset* m_Dataset;
};

class Dataset
{
public:
	Dataset() = default;

	Dataset(const std::string& pathPrefix,
			const std::string& pathSuffix,
			float particleRadius,
			float particleRadiusMultiplier,
			int count = -1);

	// generates a fluid block by randomly sampling a cube
	static void makeCube(Dataset& dataset,
						 float extent,
						 size_t numParticles,
						 float particleRadius,
						 float particleRadiusMultiplier);

	~Dataset();

	std::vector<uint32_t> GetNeighbors(const glm::vec3& position, uint32_t index);
	std::vector<uint32_t> GetNeighborsExt(const glm::vec3& position, uint32_t frame);

	float ParticleRadius;
	float ParticleRadiusExt;
	float ParticleRadiusInv;
	float ParticleRadiusExtInv;

	std::vector<Frame> Frames;

	size_t MaxParticles = 0;
	bool Loaded = false;

	CubicSplineKernel m_IsotropicKernel;
	AnisotropicKernel m_AnisotropicKernel;

private:
	void ReadFile(Partio::ParticlesDataMutable* file);
};
