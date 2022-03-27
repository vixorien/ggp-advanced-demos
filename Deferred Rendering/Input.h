#pragma once

#include <Windows.h>

class Input
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static Input& GetInstance()
	{
		if (!instance)
		{
			instance = new Input();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	Input(Input const&) = delete;
	void operator=(Input const&) = delete;

private:
	static Input* instance;
	Input() {};
#pragma endregion

public:
	~Input();

	void Initialize(HWND windowHandle);
	void Update();
	void EndOfFrame();

	int GetMouseX();
	int GetMouseY();
	int GetMouseXDelta();
	int GetMouseYDelta();
	float GetMouseWheel();
	void SetWheelDelta(float delta);

	bool KeyDown(int key);
	bool KeyUp(int key);

	bool KeyPress(int key);
	bool KeyRelease(int key);

	bool GetKeyArray(bool* keyArray, int size = 256);

	bool MouseLeftDown();
	bool MouseRightDown();
	bool MouseMiddleDown();

	bool MouseLeftUp();
	bool MouseRightUp();
	bool MouseMiddleUp();

	bool MouseLeftPress();
	bool MouseLeftRelease();

	bool MouseRightPress();
	bool MouseRightRelease();

	bool MouseMiddlePress();
	bool MouseMiddleRelease();

	void SetGuiKeyboardCapture(bool capture) { guiWantsKeyboard = capture; }
	void SetGuiMouseCapture(bool capture) { guiWantsMouse = capture; }

private:
	// Arrays for the current and previous key states
	unsigned char* kbState;
	unsigned char* prevKbState;

	// Mouse position and wheel data
	int mouseX;
	int mouseY;
	int prevMouseX;
	int prevMouseY;
	int mouseXDelta;
	int mouseYDelta;
	float wheelDelta;

	// The window's handle (id) from the OS, so
	// we can get the cursor's position
	HWND windowHandle;

	bool guiWantsKeyboard;
	bool guiWantsMouse;
};

