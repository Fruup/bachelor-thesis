#pragma once

template <typename value_t, uint32_t Capacity>
class StackArray
{
public:
	StackArray() = default;
	StackArray(uint32_t size): m_Size(size) {}

public:
	void resize(uint32_t size)
	{
		m_Size = size;
	}

	void push(const value_t& v)
	{
#if !PRODUCTION
		HZ_ASSERT(m_Size < Capacity, "Capacity exceeded!");
#endif

		m_Data[m_Size++] = v;
	}

	uint32_t size() { return m_Size; }
	value_t* data() { return m_Data; }

	value_t& operator [](uint32_t i) { return m_Data[i]; }

private:
	value_t m_Data[Capacity];
	uint32_t m_Size = 0;
};
