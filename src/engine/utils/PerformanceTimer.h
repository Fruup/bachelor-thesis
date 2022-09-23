#pragma once

#include <spdlog/spdlog.h>
#include <chrono>

#include <engine/utils/Statistics.h>

class PerformanceTimer
{
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::time_point<Clock>;
	using DurationUnit = std::chrono::milliseconds;
public:
	PerformanceTimer(const char* name) :
		Name(name)
	{
		Start = Clock::now();
	}

	~PerformanceTimer()
	{
		TimePoint end = Clock::now();
		auto durationCount = (end - Start).count();

		double duration = double(durationCount);
		const char* unit = "ns";

		if (durationCount >= 1000000)
		{
			auto natural = durationCount / 1000000;
			duration = double(natural) + 1e-3 * double(durationCount / 1000 - natural);
			unit = "ms";
		}
		else if (durationCount >= 1000)
		{
			auto natural = durationCount / 1000;
			duration = double(natural) + 1e-3 * double(durationCount - natural);
			unit = "us";
		}

		char result[128];
		sprintf_s(result, "%.3f %s", duration, unit);

		GlobalStatistics.SetTimer(Name, result);

		// spdlog::info("PERFORMANCE ({}): {:.2f} {}", Name, duration, unit);
	}

	TimePoint Start;
	const char* Name;
};
