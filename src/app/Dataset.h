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
		//file->attributeInfo("velocity", attrVelocity);

		Snapshot snapshot;
		snapshot.reserve(file->numParticles());

		for (int i = 0; i < file->numParticles(); i++)
		{
			// compute density
			const float* position = file->data<float>(attrPosition, i);
			//const float* velocity = file->data<float>(attrPosition, i);

			/*std::vector<Partio::ParticleIndex> points;
			std::vector<float> squaredDistances;

			const int N = 30;
			const float MAX_RADIUS = 1.0f;
			auto n = file->findNPoints(position, N, MAX_RADIUS, points, squaredDistances);

			float density = float(n) / float(N);*/

			snapshot.push_back(Particle{
				.Position = glm::vec3(position[0], position[1], position[2]),
				//.Velocity = glm::vec3(velocity[0], velocity[1], velocity[2]),
				//.Density = density,
			});
		}

		Snapshots.push_back(snapshot);
	}
};
