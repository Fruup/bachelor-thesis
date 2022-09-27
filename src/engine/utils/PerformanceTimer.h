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
		Storage.Timers[Name].Add(end - Start);
	}

	const char* Name;
	TimePoint Start;

public:
	template <size_t Size>
	struct Timeline
	{
		Timeline()
		{
			for (size_t i = 0; i < Size; ++i)
				Values[i] = Duration::min();
		}

		Duration GetAverage() const
		{
			Duration r = Values[0];
			for (size_t i = 1; i < Size; ++i) r += Values[i];
			return r / Size;
		}

		void Add(Duration x)
		{
			Values[Index++ % Size] = x;
		}

		int Index = 0;
		Duration Values[Size];
	};

	static struct _Storage
	{
		std::unordered_map<const char*, Timeline<60>> Timers;
	} Storage;

	static std::string Stringify(const char* timer)
	{
		Duration average = Storage.Timers[timer].GetAverage();
		return Stringify(average);
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
			s << double(c) / double(1000) << "mu";
		else
			s << c << "ns";

		return s.str();
	}
};

#define PROFILE_SCOPE_LINE2(name, line) PerformanceTimer timer_##line(name)
#define PROFILE_SCOPE_LINE(name, line) PROFILE_SCOPE_LINE2(name, line)

#define PROFILE_SCOPE(name) PROFILE_SCOPE_LINE(name, __LINE__)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
