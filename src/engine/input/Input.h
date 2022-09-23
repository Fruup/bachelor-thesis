#pragma once

#include "engine/utils/CursorPosition.h"

struct InputData
{
	bool LeftClick = false;
	bool RightClick = false;

	::CursorPosition CursorPosition;
	::CursorPosition LeftClickPosition;
	::CursorPosition RightClickPosition;
};

class Input
{
public:
	static void Init();
	static void Update(float time);

	static CursorPosition GetCursorPosition() { return Data.CursorPosition; }

	static InputData Data;
};
