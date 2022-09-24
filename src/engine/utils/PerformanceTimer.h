#pragma once

class PerformanceTimer
{
public:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::high_resolution_clock::time_point;
	using Duration = std::chrono::high_resolution_clock::duration;

public:
	PerformanceTimer(const char* name) : Name(name)
	{
		Start = Clock::now();
	}

	~PerformanceTimer()
	{
		TimePoint end = Clock::now();
		Storage.Timers[Name] = end - Start;
	}

	const char* Name;
	TimePoint Start;

public:
	static struct _Storage
	{
		std::unordered_map<const char*, Duration> Timers;
	} Storage;

	static void PrintAll()
	{
		for (const auto& [name, duration] : Storage.Timers)
			SPDLOG_INFO("Timer '{}': {}ms", name, double(duration.count()) / double(1000000));
	}

	static std::string Stringify(const Duration& duration)
	{
		const auto c = duration.count();
		std::stringstream s;

		if (c > 1000000000)
			s << double(c) / double(1000000000) << "s";
		else if (c > 1000000)
			s << double(c) / double(1000000) << "ms";
		else if (c > 1000)
			s << double(c) / double(1000) << "µs";
		else
			s << c << "ns";

		return s.str();
	}
};

#define PROFILE_SCOPE_LINE2(name, line) PerformanceTimer timer_##line(name)
#define PROFILE_SCOPE_LINE(name, line) PROFILE_SCOPE_LINE2(name, line)

#define PROFILE_SCOPE(name) PROFILE_SCOPE_LINE(name, __LINE__)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
