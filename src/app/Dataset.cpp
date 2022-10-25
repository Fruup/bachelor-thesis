#include <engine/hzpch.h>

#include "Dataset.h"

#include "app/Utils.h"

// -------------------------------------------------------------------

Frame::Frame(Dataset* dataset,
			 const std::vector<Particle>& particles,
			 float neighborhoodRadius,
			 float neighborhoodRadiusExt) :
	m_Dataset(dataset),
	m_Particles(particles),
	m_ParticlesExt(particles),
	m_Search(neighborhoodRadius),
	m_SearchExt(neighborhoodRadiusExt)
{
	HZ_ASSERT(sizeof(Particle) == sizeof(float) * 3, "Check memory alignment!");

	BuildSearch();
	ComputeAABB();
	BuildDensityGrid(1/* * m_Dataset->m_IsotropicKernel.W0()*/);
}

OctreeNode* Frame::QueryDensityGrid(const glm::vec3& p)
{
	glm::vec3 floor = glm::floor((p - m_Min) * m_DensityGrid.m_InvCellWidthVec);

	auto x = int32_t(floor.x);
	auto y = int32_t(floor.y);
	auto z = int32_t(floor.z);

	if (x >= 0 && x < m_DensityGrid.m_Width &&
		y >= 0 && y < m_DensityGrid.m_Height &&
		z >= 0 && z < m_DensityGrid.m_Depth)
	{
		auto index =
			x +
			y * m_DensityGrid.m_Width +
			z * m_DensityGrid.m_Width * m_DensityGrid.m_Height;

		return &m_DensityGrid.m_Nodes[index];
	}

	return nullptr;
}

void Frame::BuildSearch()
{
	{
		HZ_ASSERT(sizeof(Particle) == sizeof(float) * 3, "!");

		int pointSet = m_Search.add_point_set(
			(float*)m_Particles.data(), m_Particles.size(), false, false, false);

		// sort for quick access
		m_Search.z_sort();
		m_Search
			.point_set(pointSet)
			.sort_field(m_Particles.data()); // this changes the order of the data
		m_Search.update_point_sets();
	}

	{
		int pointSet = m_SearchExt.add_point_set(
			(float*)m_ParticlesExt.data(), m_ParticlesExt.size(), false, true);

		// sort for quick access
		m_SearchExt.z_sort();
		m_SearchExt
			.point_set(pointSet)
			.sort_field(m_ParticlesExt.data()); // this changes the order of the data
		m_SearchExt.update_point_sets();
	}
}

void Frame::ComputeAABB()
{
	m_Max = m_Particles[0];
	m_Min = m_Particles[0];

	for (const auto& p : m_Particles)
	{
		m_Max = glm::max(m_Max, p);
		m_Min = glm::min(m_Min, p);
	}

	const glm::vec3 padding = 1.0f * glm::vec3(m_Dataset->ParticleRadius);
	m_Max += padding;
	m_Min -= padding;
}

void Frame::BuildDensityGrid(float isoDensity)
{
	const float cellWidth = 1.0f * m_Dataset->ParticleRadius;
	m_DensityGrid.m_CellWidth = cellWidth;
	m_DensityGrid.m_InvCellWidthVec = 1.0f / glm::vec3(cellWidth);

	const glm::vec3 aabb = m_Max - m_Min;

	auto width  = m_DensityGrid.m_Width  = int32_t(std::ceil(aabb.x / cellWidth));
	auto height = m_DensityGrid.m_Height = int32_t(std::ceil(aabb.y / cellWidth));
	auto depth  = m_DensityGrid.m_Depth  = int32_t(std::ceil(aabb.z / cellWidth));

	auto getNode = [&](int32_t x, int32_t y, int32_t z) -> OctreeNode*
	{
		if (x >= 0 && x < width &&
			y >= 0 && y < height &&
			z >= 0 && z < depth)
			return &m_DensityGrid.m_Nodes[x + y * width + z * width * height];
		return nullptr;
	};

	m_DensityGrid.m_Nodes.resize(width * height * depth);

	// determine number of particles in cells
	for (uint32_t x = 0; x < width; x++)
		for (uint32_t y = 0; y < height; y++)
			for (uint32_t z = 0; z < depth; z++)
			{
				const glm::vec3 p =
					m_Min + (glm::vec3(x, y, z) + glm::vec3(0.5f)) * cellWidth;
				
				OctreeNode* node = getNode(x, y, z);

				std::vector<std::vector<uint32_t>> _neighbors;
				m_Search.find_neighbors_box((float*)&p, _neighbors);

				node->ParticleIndices = std::move(_neighbors[0]);
				node->NumParticles = node->ParticleIndices.size();
				node->Min = m_Min + glm::vec3(x, y, z) * cellWidth;
				node->Max = node->Min + glm::vec3(cellWidth);
			}

	// determine density mask
	for (uint32_t x = 0; x < width; x++)
		for (uint32_t y = 0; y < height; y++)
			for (uint32_t z = 0; z < depth; z++)
			{
				float N_c = 0.0f;
				
				for (int32_t dx = -1; dx <= 1; dx++)
					for (int32_t dy = -1; dy <= 1; dy++)
						for (int32_t dz = -1; dz <= 1; dz++)
						{
							const OctreeNode* node = getNode(x + dx, y + dy, z + dz);

							if (node)
							{
								const float C = 1000.0f;
								const float r = float(std::abs(dx) + std::abs(dy) + std::abs(dz));
								const float weight = std::exp(-C * r);

								N_c += weight * float(node->NumParticles);
							}
						}

				// approximate density (eq. 3)
				float rho = N_c * m_Dataset->m_IsotropicKernel.W0();

				OctreeNode* node = getNode(x, y, z);
				node->Flag = rho > isoDensity;
			}
}

// -------------------------------------------------------------------

Dataset::Dataset(const std::string& pathPrefix,
				 const std::string& pathSuffix,
				 float particleRadius,
				 float particleRadiusMultiplier,
				 int count) :
	ParticleRadius(particleRadius),
	ParticleRadiusExt(particleRadiusMultiplier * ParticleRadius),
	ParticleRadiusInv(1.0f / ParticleRadius),
	ParticleRadiusExtInv(1.0f / ParticleRadiusExt),
	m_AnisotropicKernel(ParticleRadius),
	m_IsotropicKernel(ParticleRadius)
{
	std::string path;
	int i = 1;

	SPDLOG_INFO("Reading dataset '{}X{}'...", pathPrefix, pathSuffix);

	// collect file names
	std::vector<std::string> fileList;
	while (count < 0 || i <= count)
	{
		path = pathPrefix;
		path.append(std::to_string(i));
		path.append(pathSuffix);

		AssetPathAbs absolutePath(path);

		if (!absolutePath.Exists())
			break;

		fileList.push_back(absolutePath.string());

		++i;
	}

	// This is important because once a frame is present in memory,
	// it can not be moved to a different address.
	// Thus, we have to prevent the vector from reallocating.
	Frames.reserve(fileList.size());
	for (const auto& filename : fileList)
	{
		const auto file = Partio::read(filename.c_str());
		if (!file)
		{
			SPDLOG_ERROR("Failed to load particle file!");
			return;
		}

		// read in particle data
		ReadFile(file);
		file->release();
	}

	MaxParticles = 0;
	for (const auto& s : Frames)
		MaxParticles = std::max(MaxParticles, s.m_Particles.size());

	Loaded = true;
}

void Dataset::makeCube(Dataset& r,
					   float extent,
					   size_t numParticles,
					   float particleRadius,
					   float particleRadiusMultiplier)
{
	r.ParticleRadius = particleRadius;
	r.ParticleRadiusExt = particleRadiusMultiplier * particleRadius;
	r.ParticleRadiusInv = 1.0f / particleRadius;
	r.ParticleRadiusExtInv = 1.0f / r.ParticleRadiusExt;
	r.m_IsotropicKernel = CubicSplineKernel(r.ParticleRadius);
	r.m_AnisotropicKernel = AnisotropicKernel(r.ParticleRadius);

	std::vector<Particle> particles;
	for (size_t i = 0; i < numParticles; i++)
	{
		particles.push_back({
			RandomFloat(-extent, extent),
			RandomFloat(-extent, extent),
			RandomFloat(-extent, extent),
		});
	}

	particles.push_back({
		0, 2 * extent, 0
	});

	r.Frames.emplace_back(&r, particles, r.ParticleRadius, r.ParticleRadiusExt);

	r.MaxParticles = 0;
	for (const auto& s : r.Frames)
		r.MaxParticles = std::max(r.MaxParticles, s.m_Particles.size());

	r.Loaded = true;
}

Dataset::~Dataset()
{
	Frames.clear();

	Loaded = false;
}

std::vector<uint32_t> Dataset::GetNeighbors(const glm::vec3& p, uint32_t frameIndex)
{
	std::vector<std::vector<uint32_t>> neighbors;

	std::array<float, 3> _p = { p[0], p[1], p[2]};
	Frames[frameIndex].m_Search.find_neighbors(_p.data(), neighbors);

	return neighbors[0];
}

std::vector<uint32_t> Dataset::GetNeighborsExt(const glm::vec3& p, uint32_t frameIndex)
{
	std::vector<std::vector<uint32_t>> neighbors;

	std::array<float, 3> _p = { p[0], p[1], p[2]};
	Frames[frameIndex].m_SearchExt.find_neighbors(_p.data(), neighbors);

	return neighbors[0];
}

void Dataset::ReadFile(Partio::ParticlesDataMutable* file)
{
	Partio::ParticleAttribute attrPosition;
	file->attributeInfo("position", attrPosition);

	std::vector<Particle> particles;

	for (int i = 0; i < file->numParticles(); i++)
	{
		const Particle* particle = file->data<Particle>(attrPosition, i);
		particles.emplace_back(*particle);
	}

	Frames.emplace_back(this, particles, ParticleRadius, ParticleRadiusExt);
}
