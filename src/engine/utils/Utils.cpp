#include "engine/hzpch.h"
#include "engine/utils/Utils.h"

namespace Utils
{
	std::stack<RandomSession*> RandomSession::s_SessionStack;
	RandomSession RandomSession::s_globalSession;
}
