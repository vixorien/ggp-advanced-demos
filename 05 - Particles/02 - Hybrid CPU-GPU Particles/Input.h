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

	void ProcessRawMouseInput(LPARAM input);
	int GetRawMouseXDelta();
	int GetRawMouseYDelta();

	float GetMouseWheel();
	void SetWheelDelta(float delta);

	void SetKeyboardCapture(bool captured);
	void SetMouseCapture(bool captured);

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

private:
	// Arrays for the current and previous key states
	unsigned char* kbState {0};
	unsigned char* prevKbState {0};

	// Mouse position and wheel data
	int mouseX {0};
	int mouseY {0};
	int prevMouseX {0};
	int prevMouseY {0};
	int mouseXDelta {0};
	int mouseYDelta {0};
	int rawMouseXDelta{ 0 };
	int rawMouseYDelta{ 0 };
	float wheelDelta {0};

	// Support for capturing input outside the input manager
	bool keyboardCaptured {0};
	bool mouseCaptured {0};

	// The window's handle (id) from the OS, so
	// we can get the cursor's position
	HWND windowHandle {0};
};

