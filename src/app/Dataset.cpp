#include <engine/hzpch.h>

#include "Dataset.h"

#include "app/Utils.h"

Dataset::Dataset(const std::string& pathPrefix, const std::string& pathSuffix, int startIndex) :
	//ParticleRadius(0.025f)
	ParticleRadius(0.1f),
	ParticleRadiusExt(2.0f * ParticleRadius),
	ParticleRadiusInv(1.0f / ParticleRadius),
	ParticleRadiusExtInv(1.0f / ParticleRadiusExt)
{
	std::string path;
	int i = startIndex;

	SPDLOG_INFO("Reading dataset '{}X{}'...", pathPrefix, pathSuffix);

	while (true)
	{
		path = pathPrefix;
		path.append(std::to_string(i));
		path.append(pathSuffix);

		AssetPathAbs absolutePath(path);

		if (!absolutePath.Exists())
			break;

		auto file = Partio::read(absolutePath.string().c_str());
		if (!file) return;

		// read in particle data
		ReadFile(file);
		file->release();

		// build search structure
		BuildCompactNSearch();

		++i;
	}

	MaxParticles = 0;
	for (const auto& s : Frames)
		MaxParticles = std::max(MaxParticles, s.size());

	Loaded = true;
}

Dataset::Dataset(float extent, size_t numParticles) :
	ParticleRadius(0.1f)
{
	Frame s;

	for (size_t i = 0; i < numParticles; i++)
	{
		s.push_back({
			RandomFloat(-extent, extent),
			RandomFloat(-extent, extent),
			RandomFloat(-extent, extent),
		});
	}

	Frames.push_back(s);

	// build search structure
	BuildCompactNSearch();

	MaxParticles = 0;
	for (const auto& s : Frames)
		MaxParticles = std::max(MaxParticles, s.size());

	Loaded = true;
}

Dataset::~Dataset()
{
	Frames.clear();
	FramesExt.clear();
	NSearch.clear();
	NSearchExt.clear();

	Loaded = false;
}

std::vector<uint32_t> Dataset::GetNeighbors(const glm::vec3& position, uint32_t index)
{
	std::vector<std::vector<uint32_t>> neighbors;
	NSearch[index].find_neighbors((float*)&position, neighbors);

	return neighbors[0];
}

std::vector<uint32_t> Dataset::GetNeighborsExt(const glm::vec3& position, uint32_t index)
{
	std::vector<std::vector<uint32_t>> neighbors;
	NSearchExt[index].find_neighbors((float*)&position, neighbors);

	return neighbors[0];
}

void Dataset::ReadFile(Partio::ParticlesDataMutable* file)
{
	Partio::ParticleAttribute attrPosition, attrVelocity;

	file->attributeInfo("position", attrPosition);

	Frames.push_back(Frame());
	Frame& frame = Frames.back();
	frame.reserve(file->numParticles());

	for (int i = 0; i < file->numParticles(); i++)
	{
		const float* position = file->data<float>(attrPosition, i);
		//glm::vec3 v(position[0], position[1], position[2]);

		frame.push_back(Particle{ position[0], position[1], position[2] });
	}

	// add copy
	FramesExt.push_back(frame);
}

void Dataset::BuildCompactNSearch()
{
		// 1

	//CompactNSearch::NeighborhoodSearch search(ParticleRadius);
	//const auto ps = search.add_point_set((float*)Frames.back().data(), Frames.back().size(), false, true);
	//
	//// sort for quick access

	//search.z_sort();
	//search.point_set(ps).sort_field(Frames.back().data());

	//search.update_point_sets(); // this changes the order of the data in 'Frames'

	//NSearch.push_back(search);

		// 2

	CompactNSearch::NeighborhoodSearch searchExt(ParticleRadiusExt);
	const auto psExt = searchExt.add_point_set((float*)Frames.back().data(), Frames.back().size(), false, true);

	// sort for quick access

	searchExt.z_sort();
	searchExt.point_set(psExt).sort_field(Frames.back().data());

	searchExt.update_point_sets(); // this changes the order of the data in 'FramesExt'

	NSearchExt.push_back(searchExt);
}
