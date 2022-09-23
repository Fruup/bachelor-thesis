#pragma once

#include "engine/utils/Utils.h"

#include <unordered_map>

class PerformanceTimer;

// https://www.c-plusplus.net/forum/topic/348084/compile-time-string-hash/3
constexpr uint32_t fnv32(const char* s)
{
	uint32_t hash{ 2166136261u };
	while (*s) hash = 16777619u * (hash ^ static_cast<uint32_t>(*s++));
	return hash;
}

class Statistics
{
public:
	struct Entry
	{
		static constexpr size_t MaxSamples = 32;
		size_t NumSamples = 0;
		size_t Index = 0;
		//std::array<float, MaxSamples> Samples;
		float Samples[MaxSamples];

		void Sample(float data);

		float GetBack() const;
		float GetAverage() const;
		float GetMin() const;
		float GetMax() const;
	};
	
	struct Timer
	{
		char Name[64];
		char Value[64];
	};

public:
	Statistics(const std::string& name) :
		m_StatsName(name)
	{
	}

	void Begin();
	void ImGuiRender();

	Entry& Get(const char* name) { return Get(Utils::HashString(name)); }
	Entry& Get(size_t hash);

	void Sample(const char* name, float data)
	{
		size_t hash = Utils::HashString(name);
		m_Names[hash] = name;

		Sample(hash, data);
	}

	void Sample(size_t hash, float data);

	void CountUp(const char* name, size_t amount = 1)
	{
		size_t hash = Utils::HashString(name);
		m_Names[hash] = name;
		
		CountUp(hash, amount);
	}

	void CountUp(size_t hash, size_t amount = 1);

	void SetTimer(const char* name, const char* value);

private:
	std::unordered_map<size_t, Entry> m_Entries;
	std::unordered_map<size_t, size_t> m_Counters;
	std::unordered_map<size_t, std::string> m_Names;

	std::unordered_map<const char*, Timer> m_Timers;

	std::string m_StatsName;
	bool m_Open = true;
};

// global statistics instance

extern Statistics GlobalStatistics;
