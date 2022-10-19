#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <barrier>

class ThreadPool
{
	using F = std::function<void(uint32_t)>;

public:
	ThreadPool(uint32_t numThreads, const F& f = {});

	void Exit();

	void Start(uint32_t problemSize);

	bool IsDone();

	void SetFunction(const F& f) { m_Function = f; }

private:
	void Worker();

private:
	std::condition_variable m_ConditionVariable;
	std::mutex m_Mutex;

	std::barrier<> m_Barrier;

	bool m_Exit = false;

	bool m_Started = false;

	uint32_t m_NumThreads;

	std::vector<std::thread> m_Threads;

	F m_Function;

	uint32_t m_ProblemSize;
	std::atomic_uint32_t m_ProblemPointer = 0;
};
