#pragma once

#include <string>
#include <vector>
#include <random>
#include <stack>
#include <chrono>

namespace Utils
{
	class RandomSession {
	public:
		static std::stack<RandomSession*> s_SessionStack;
		static RandomSession s_globalSession;
	public:
		RandomSession(unsigned int seed = 0) {
			// push to stack
			s_SessionStack.push(this);

			// set seed
			m_engine.seed(seed == 0 ? unsigned int(std::chrono::high_resolution_clock::now().time_since_epoch().count()) : seed);
		}

		~RandomSession() {
			// pop from stack
			s_SessionStack.pop();
		}

		std::mt19937 m_engine;
	};

	static int RandomInt(int min, int max) {
		return std::uniform_int_distribution(min, max)(RandomSession::s_SessionStack.top()->m_engine);
	}

	static bool RandomChance(int chance) {
		return RandomInt(0, 99) < chance;
	}

	// ----------------------------------

	// not case sensitive, no spaces or tabs
	static constexpr size_t HashString(const char* const& s)
	{
		size_t len = 0;
		while (s[len] != 0) len++;

		const size_t p = 31;
		const size_t m = size_t(1e9) + 9;
		size_t hash_value = 0;
		size_t p_pow = 1;

		for (size_t i = 0; i < len; ++i)
		{
			if (!std::isblank(s[i]))
			{
				hash_value = (hash_value + size_t(std::tolower(s[i]) - 'a' + 1) * p_pow) % m;
				p_pow = (p_pow * p) % m;
			}
		}

		return hash_value;
	}

	static std::string ToLower(const std::string& s)
	{
		std::string r = s;
		for (size_t i = 0; i < r.length(); i++)
			r[i] = char(std::tolower(s[i]));
		return r;
	}

	static std::string& TrimWhitespaces(std::string& str)
	{
		size_t s, f;

		for (s = 0; s < str.length() && std::isspace(str[s]); s++) {}
		for (f = str.length() - 1; f >= 0 && isspace(str[f]); f--) {}

		str = str.substr(s, f - s + 1);
		return str;
	}

	static bool CompareStrings(const std::string& a, const std::string& b)
	{
		if (a.length() != b.length()) return false;
		for (size_t i = 0; i < a.length(); i++)
			if (std::tolower(a[i]) != std::tolower(b[i]))
				return false;
		return true;
	}

	static std::string CorrectSlashes(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](char c) -> char { return c == '\\' ? '/' : c; });
		return s;
	}

	static size_t FindFirstOf(const std::string& s, std::initializer_list<const char*> delimiters, int* indexOut = nullptr)
	{
		//HZ_ASSERT(delimiters.size() != 0, "Delimiter invalid!");

		size_t r = std::string::npos;
		for (int i = 0; i < delimiters.size(); i++)
		{
			//HZ_ASSERT(delimiters.begin()[i][0] != 0, "Delimiter invalid!");

			r = std::min(r, s.find_first_of(delimiters.begin()[i]));
			if (indexOut) *indexOut = i;
		}
		return r;
	}

	static std::vector<std::string> SplitString(std::string s, std::initializer_list<const char*> delimiters)
	{
		std::vector<std::string> r;
		size_t c = std::string::npos;
		int index;

		while ((c = FindFirstOf(s, delimiters, &index)) != std::string::npos)
		{
			const std::string& sub = s.substr(0, c);
			if (!sub.empty())
			{
				r.push_back(sub);
				s = s.substr(c + strlen(delimiters.begin()[index]));
			}
		}

		// add last piece
		if (!s.empty()) r.push_back(s);

		return r;
	}

	static std::string MergePaths(const std::string& path, const std::string& relative)
	{
		/*
		"assets/maps/chapter1/"
		"../files/hello.txt"

		===> "assets/files/hello.txt" */

		auto pathSplits = SplitString(path, { "/", "\\" });
		auto relSplits = SplitString(relative, { "/", "\\" });

		// move back (relative)
		int i = 0;
		for (; i < relSplits.size() && relSplits[i] == ".."; ++i)
			pathSplits.pop_back();

		// then append rest
		for (; i < relSplits.size(); ++i)
			pathSplits.push_back(relSplits[i]);

		// construct result
		std::string r;
		for (int i = 0; i < pathSplits.size(); ++i) {
			r += pathSplits[i];
			if (i != pathSplits.size() - 1) r += '/';
		}

		if (relative.back() == '/')
			r += '/';

		return r;
	}

	static constexpr uint32_t _FuzzyCompare(const char* a, size_t la, const char* b, size_t lb)
	{
		if (la < lb)
			return _FuzzyCompare(b, lb, a, la);

		if (la == lb)
		{
			uint32_t r = 0;

			for (size_t i = 0; i < la; i++)
				if (a[i] - b[i] != 0)
					r++;

			return r;
		}

		size_t i = 0;
		while (i < lb && a[i] == b[i])
			++i;

		if (i == lb)
			return uint32_t(la - i);
		else
			return 1 + _FuzzyCompare(a + i + 1, la - i - 1, b + i, lb - i);
	}

	static uint32_t FuzzyCompare(const std::string& a, const std::string& b)
	{
		std::string a_;
		for (const char& c : a)
			if (!std::isblank(c))
				a_.push_back(std::tolower(c));

		std::string b_;
		for (const char& c : b)
			if (!std::isblank(c))
				b_.push_back(std::tolower(c));

		return _FuzzyCompare(a_.c_str(), a_.length(), b_.c_str(), b_.length());
	}

	template <typename T>
	static const T& GetBestFit(const std::vector<T>& a, const std::function<const char* (T&)>& getName, const std::string& name)
	{
		uint32_t m = UINT32_MAX;
		size_t mi = SIZE_MAX;

		for (size_t i = 0; i < a.size(); ++i)
		{
			uint32_t d = Utils::FuzzyCompare(getName(a[i]), name);

			if (d < m)
			{
				m = d;
				mi = i;
			}
		}

		return a[mi];
	}
}
