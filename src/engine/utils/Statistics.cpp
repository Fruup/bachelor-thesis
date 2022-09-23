#include "engine/hzpch.h"
#include "engine/utils/Statistics.h"
#include "engine/utils/PerformanceTimer.h"

Statistics GlobalStatistics("Global");

void Statistics::Entry::Sample(float data)
{
	Samples[Index] = data;
	NumSamples = std::min(MaxSamples, NumSamples + 1);
	Index = (Index + 1) % MaxSamples;
}

float Statistics::Entry::GetBack() const
{
	HZ_ASSERT(NumSamples, "Entry has no samples!");
	return Samples[Index];
}

float Statistics::Entry::GetAverage() const
{
	if (NumSamples == 0)
		return 0.0f;

	float sum = 0.0f;
	for (size_t i = 0; i < NumSamples; i++)
		sum += Samples[i];
	return sum / float(NumSamples);
}

float Statistics::Entry::GetMin() const
{
	if (NumSamples == 0)
		return 0.0f;

	float min = Samples[0];
	for (size_t i = 1; i < NumSamples; i++)
		min = std::min(min, Samples[i]);
	return min;
}

float Statistics::Entry::GetMax() const
{
	if (NumSamples == 0)
		return 0.0f;

	float max = Samples[0];
	for (size_t i = 1; i < NumSamples; i++)
		max = std::max(max, Samples[i]);
	return max;
}

void Statistics::Begin()
{
	// reset counters
	for (auto& [k, v] : m_Counters)
		v = 0;
}

Statistics::Entry& Statistics::Get(size_t hash)
{
	auto it = m_Entries.find(hash);

	if (it == m_Entries.end())
	{
		m_Entries[hash] = {};
		return m_Entries[hash];
	}
	else return it->second;
}

void Statistics::Sample(size_t hash, float data)
{
	Get(hash).Sample(data);
}

void Statistics::CountUp(size_t hash, size_t amount)
{
	m_Counters[hash] += amount;
}

void Statistics::ImGuiRender()
{
	std::string strId = "Statistics - " + m_StatsName;

	ImGui::Begin(strId.c_str(), &m_Open);

	if (m_Entries.empty() && m_Counters.empty())
	{
		ImGui::Text("Nothing to display");
	}
	else
	{
		ImGui::BeginTable(strId.c_str(), 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg);

		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		for (auto& [hash, entry] : m_Entries)
		{
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text((m_Names[hash] + ":").c_str());

			ImGui::TableNextColumn();
			if (entry.NumSamples)
				ImGui::Text(std::to_string(entry.GetBack()).c_str());
			else ImGui::Text("-");
		}

		for (auto& [hash, counter] : m_Counters)
		{
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text((m_Names[hash] + ":").c_str());

			ImGui::TableNextColumn();
			ImGui::Text(std::to_string(counter).c_str());
		}

		ImGui::EndTable();
	}

	ImGui::End();

	ImGui::Begin("Timers");
	
	{
		for (const auto& [k, timer] : m_Timers)
		{
			ImGui::Text("%s:", timer.Name);
			ImGui::Text("  %s", timer.Value);
		}
	}

	ImGui::End();
}

void Statistics::SetTimer(const char* name, const char* value)
{
	auto it = m_Timers.find(name);

	if (it == m_Timers.end())
	{
		auto& timer = m_Timers[name];
		strcpy_s(timer.Name, name);
		strcpy_s(timer.Value, value);
	}
	else
	{
		strcpy_s(it->second.Value, value);
	}
}
