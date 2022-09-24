#pragma once

#include <Partio.h>
#include <CompactNSearch.h>

#include <engine/assets/AssetPath.h>

struct Particle
{
	glm::vec3 Position;
};

class Dataset
{
	using Snapshot = std::vector<Particle>;

public:
	Dataset(const std::string& pathPrefix, const std::string& pathSuffix = ".bgeo", int startIndex = 1)
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

		Loaded = true;
	}

	~Dataset()
	{
		Snapshots.clear();
		NSearch.clear();

		Loaded = false;
	}

	std::vector<uint32_t> GetNeighbors(const glm::vec3& position, uint32_t index)
	{
		std::vector<std::vector<uint32_t>> neighbors;
		NSearch[index].find_neighbors(position.data.data, neighbors);

		return neighbors[0];
	}

	const float ParticleRadius = 0.1f;

	std::vector<Snapshot> Snapshots;
	std::vector<CompactNSearch::NeighborhoodSearch> NSearch;

	bool Loaded = false;

private:
	void ReadFile(Partio::ParticlesDataMutable* file)
	{
		Partio::ParticleAttribute attrPosition, attrVelocity;

		file->attributeInfo("position", attrPosition);

		Snapshot snapshot;
		snapshot.reserve(file->numParticles());

		for (int i = 0; i < file->numParticles(); i++)
		{
			const float* position = file->data<float>(attrPosition, i);
			glm::vec3 v(position[0], position[1], position[2]);

			snapshot.push_back(Particle{ v });
		}

		Snapshots.push_back(snapshot);
	}

	void BuildCompactNSearch()
	{
		CompactNSearch::NeighborhoodSearch search(ParticleRadius);
		const auto ps = search.add_point_set((float*)Snapshots.back().data(), Snapshots.back().size());

		// sort for quick access
		search.z_sort();
		search.point_set(ps).sort_field(Snapshots.back().data());
		
		search.update_point_sets();

		NSearch.push_back(search);
	}
};
