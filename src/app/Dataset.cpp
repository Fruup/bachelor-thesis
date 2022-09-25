#include <engine/hzpch.h>

#include "Dataset.h"

Dataset::Dataset(const std::string& pathPrefix, const std::string& pathSuffix, int startIndex) :
	//ParticleRadius(0.025f)
	ParticleRadius(0.1f)
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

	for (const auto& s : Snapshots)
		MaxParticles = std::max(MaxParticles, s.size());

	Loaded = true;
}

Dataset::~Dataset()
{
	Snapshots.clear();
	NSearch.clear();

	Loaded = false;
}

std::vector<uint32_t> Dataset::GetNeighbors(const glm::vec3& position, uint32_t index)
{
	std::vector<std::vector<uint32_t>> neighbors;
	NSearch[index].find_neighbors(position.data.data, neighbors);

	return neighbors[0];
}

void Dataset::ReadFile(Partio::ParticlesDataMutable* file)
{
	Partio::ParticleAttribute attrPosition, attrVelocity;

	file->attributeInfo("position", attrPosition);

	Snapshots.push_back(Snapshot());
	Snapshot& snapshot = Snapshots.back();
	snapshot.reserve(file->numParticles());

	for (int i = 0; i < file->numParticles(); i++)
	{
		const float* position = file->data<float>(attrPosition, i);
		//glm::vec3 v(position[0], position[1], position[2]);

		snapshot.push_back(Particle{ position[0], position[1], position[2] });
	}
}

void Dataset::BuildCompactNSearch()
{
	CompactNSearch::NeighborhoodSearch search(ParticleRadius);
	const auto ps = search.add_point_set(Snapshots.back().front().data(), Snapshots.back().size(), false, true);
	
	// sort for quick access

	search.z_sort();
	search.point_set(ps).sort_field(Snapshots.back().data());

	search.update_point_sets();

	NSearch.push_back(search);
}
