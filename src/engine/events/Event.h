#pragma once

#include <string>
#include <functional>

#define EVENT_TYPE(x) constexpr static EventType StaticType = ##x

using EventType = size_t;

class Event
{
public:
	Event(EventType type) : Type(type)
	{
	}

	virtual std::string ToString() const { return "Event<" + std::to_string(Type) + ">"; }

	const EventType Type;
	bool Handled = false;
};

class EventDispatcher
{
public:
	EventDispatcher(Event& e) : m_Event(e)
	{
	}

	template <typename T>
	bool Dispatch(const std::function<bool(T&)>& f) const
	{
		static_assert(std::is_base_of_v<Event, T>);

		if (m_Event.Type == T::StaticType)
		{
			m_Event.Handled |= f(static_cast<T&>(m_Event));
			return true;
		}

		return false;
	}

	template <typename T>
	bool Dispatch(const std::function<void(T&)>& f) const
	{
		static_assert(std::is_base_of_v<Event, T>);

		if (m_Event.Type == T::StaticType)
		{
			f(static_cast<T&>(m_Event));
			return true;
		}

		return false;
	}

private:
	Event& m_Event;
};
