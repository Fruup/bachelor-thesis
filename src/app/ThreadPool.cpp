#include <engine/hzpch.h>

#include "ThreadPool.h"

ThreadPool::ThreadPool(uint32_t numThreads, const F& f) :
	m_NumThreads(numThreads),
	m_Function(f),
	m_Barrier(numThreads + 1)
{
	m_Threads.resize(m_NumThreads);
	for (uint32_t i = 0; i < m_NumThreads; i++)
		m_Threads[i] = std::thread(&ThreadPool::Worker, this);
}

void ThreadPool::Exit()
{
	m_Exit = true;
	m_Barrier.arrive_and_wait();

	for (uint32_t i = 0; i < m_NumThreads; i++)
		if (m_Threads[i].joinable())
			m_Threads[i].join();
}

void ThreadPool::Start(uint32_t problemSize)
{
	m_ProblemSize = problemSize;
	m_ProblemPointer = 0;

	m_Barrier.arrive();
}

bool ThreadPool::IsDone()
{
	return m_ProblemPointer.load() >= m_ProblemSize;
}

void ThreadPool::Worker()
{
	while (true)
	{
		// wait
		m_Barrier.arrive_and_wait();

		if (m_Exit)
			return;

		// work
		uint32_t index;
		while ((index = m_ProblemPointer.fetch_add(1)) < m_ProblemSize - 1)
			m_Function(index);
	}
}
