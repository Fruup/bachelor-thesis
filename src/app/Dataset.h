#pragma once

#include <partio/Partio.h>

#include <engine/assets/AssetPath.h>

struct Particle
{
	glm::vec3 Position;
	//glm::vec3 Velocity;
	//float Density;
};

class Dataset
{
	using Snapshot = std::vector<Particle>;

public:
	bool Init(const std::string& pathPrefix, const std::string& pathSuffix = ".bgeo", int startIndex = 1)
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

			File = Partio::read(absolutePath.string().c_str());

			if (!File) return false;

			ReadFile(File);

			File->release();
			File = nullptr;

			++i;
		}

		return i != startIndex;
	}

	void Exit()
	{
		if (File)
		{
			File->release();
			File = nullptr;
		}

		Snapshots.clear();
	}

	std::vector<Snapshot> Snapshots;
	Partio::ParticlesDataMutable* File = nullptr;

private:
	void ReadFile(Partio::ParticlesDataMutable* file)
	{
		Partio::ParticleAttribute attrPosition, attrVelocity;

		// file->sort();

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
};
