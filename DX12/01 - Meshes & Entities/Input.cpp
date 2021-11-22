#include "Input.h"

// Singleton requirement
Input* Input::instance;

// --------------- Basic usage -----------------
// 
// The keyboard functions all take a single character
// like 'W', ' ' or '8' (which will implicitly cast 
// to an int) or a pre-defined virtual key code like
// VK_SHIFT, VK_ESCAPE or VK_TAB. These virtual key
// codes are are accessible through the Windows.h 
// file (already included in Input.h). See the 
// following for a complete list of virtual key codes:
// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
// 
// Checking if various keys are down or up:
// 
//  if (Input::GetInstance().KeyDown('W')) { }
//  if (Input::GetInstance().KeyUp('2')) { }
//  if (Input::GetInstance().KeyDown(VK_SHIFT)) { }
//
// 
// Checking if a key was initially pressed or released 
// this frame:  
// 
//  if (Input::GetInstance().KeyPressed('Q')) { }
//  if (Input::GetInstance().KeyReleased(' ')) { }
// 
// (Note that these functions will only return true on 
// the FIRST frame that a key is pressed or released.)
// 
// 
// Checking for mouse input:
// 
//  if (Input::GetInstance().MouseLeftDown()) { }
//  if (Input::GetInstance().MouseRightDown()) { }
//  if (Input::GetInstance().MouseMiddleUp()) { }
//  if (Input::GetInstance().MouseLeftPressed()) { }
//  if (Input::GetInstance().MouseRightReleased()) { }
//
// ---------------------------------------------

// -------------- Less verbose -----------------
// 
// If you'd rather not have to type Input::GetInstance()
// over and over, you can save the reference in a variable:
//
//  Input& input = Input::GetInstance();
//  if (input.KeyDown('W')) { }
//  if (input.KeyDown('A')) { }
//  if (input.KeyDown('S')) { }
//  if (input.KeyDown('D')) { }
//
// ---------------------------------------------


// --------------------------
//  Cleans up the key arrays
// --------------------------
Input::~Input()
{
	delete[] kbState;
	delete[] prevKbState;
}

// ---------------------------------------------------
//  Initializes the input variables and sets up the
//  initial arrays of key states
//
//  windowHandle - the handle (id) of the window,
//                 which is necessary for mouse input
// ---------------------------------------------------
void Input::Initialize(HWND windowHandle)
{
	kbState = new unsigned char[256];
	prevKbState = new unsigned char[256];

	memset(kbState, 0, sizeof(unsigned char) * 256);
	memset(prevKbState, 0, sizeof(unsigned char) * 256);

	wheelDelta = 0.0f;
	mouseX = 0; mouseY = 0;
	prevMouseX = 0; prevMouseY = 0;
	mouseXDelta = 0; mouseYDelta = 0;

	this->windowHandle = windowHandle;
}

// ----------------------------------------------------------
//  Updates the input manager for this frame.  This should
//  be called at the beginning of every Game::Update(), 
//  before anything that might need input
// ----------------------------------------------------------
void Input::Update()
{
	// Copy the old keys so we have last frame's data
	memcpy(prevKbState, kbState, sizeof(unsigned char) * 256);

	// Get the latest keys (from Windows)
	// Note the use of (void), which denotes to the compiler
	// that we're intentionally ignoring the return value
	(void)GetKeyboardState(kbState);

	// Get the current mouse position then make it relative to the window
	POINT mousePos = {};
	GetCursorPos(&mousePos);
	ScreenToClient(windowHandle, &mousePos);

	// Save the previous mouse position, then the current mouse 
	// position and finally calculate the change from the previous frame
	prevMouseX = mouseX;
	prevMouseY = mouseY;
	mouseX = mousePos.x;
	mouseY = mousePos.y;
	mouseXDelta = mouseX - prevMouseX;
	mouseYDelta = mouseY - prevMouseY;
}

// ----------------------------------------------------------
//  Resets the mouse wheel value at the end of the frame.
//  This cannot occur earlier in the frame, since the wheel
//  input comes from Win32 windowing messages, which are
//  handled between frames.
// ----------------------------------------------------------
void Input::EndOfFrame()
{
	// Reset wheel value
	wheelDelta = 0;
}

// ----------------------------------------------------------
//  Get the mouse's current position in pixels relative
//  to the top left corner of the window.
// ----------------------------------------------------------
int Input::GetMouseX() { return mouseX; }
int Input::GetMouseY() { return mouseY; }


// ---------------------------------------------------------------
//  Get the mouse's change (delta) in position since last
//  frame in pixels relative to the top left corner of the window.
// ---------------------------------------------------------------
int Input::GetMouseXDelta() { return mouseXDelta; }
int Input::GetMouseYDelta() { return mouseYDelta; }


// ---------------------------------------------------------------
//  Get the mouse wheel delta for this frame.  Note that there is 
//  no absolute position for the mouse wheel; this is either a
//  positive number, a negative number or zero.
// ---------------------------------------------------------------
float Input::GetMouseWheel() { return wheelDelta; }


// ---------------------------------------------------------------
//  Sets the mouse wheel delta for this frame.  This is called
//  by DXCore whenever an OS-level mouse wheel message is sent
//  to the application.  You'll never need to call this yourself.
// ---------------------------------------------------------------
void Input::SetWheelDelta(float delta)
{
	wheelDelta = delta;
}


// ----------------------------------------------------------
//  Is the given key down this frame?
//  
//  key - The key to check, which could be a single character
//        like 'W' or '3', or a virtual key code like VK_TAB,
//        VK_ESCAPE or VK_SHIFT.
// ----------------------------------------------------------
bool Input::KeyDown(int key)
{
	if (key < 0 || key > 255) return false;

	return (kbState[key] & 0x80) != 0;
}

// ----------------------------------------------------------
//  Is the given key up this frame?
//  
//  key - The key to check, which could be a single character
//        like 'W' or '3', or a virtual key code like VK_TAB,
//        VK_ESCAPE or VK_SHIFT.
// ----------------------------------------------------------
bool Input::KeyUp(int key)
{
	if (key < 0 || key > 255) return false;

	return !(kbState[key] & 0x80);
}

// ----------------------------------------------------------
//  Was the given key initially pressed this frame?
//  
//  key - The key to check, which could be a single character
//        like 'W' or '3', or a virtual key code like VK_TAB,
//        VK_ESCAPE or VK_SHIFT.
// ----------------------------------------------------------
bool Input::KeyPress(int key)
{
	if (key < 0 || key > 255) return false;

	return
		kbState[key] & 0x80 &&			// Down now
		!(prevKbState[key] & 0x80);		// Up last frame
}

// ----------------------------------------------------------
//  Was the given key initially released this frame?
//  
//  key - The key to check, which could be a single character
//        like 'W' or '3', or a virtual key code like VK_TAB,
//        VK_ESCAPE or VK_SHIFT.
// ----------------------------------------------------------
bool Input::KeyRelease(int key)
{
	if (key < 0 || key > 255) return false;

	return
		!(kbState[key] & 0x80) &&	// Up now
		prevKbState[key] & 0x80;	// Down last frame
}


// ----------------------------------------------------------
//  A utility function to fill a given array of booleans 
//  with the current state of the keyboard.  This is most
//  useful when hooking the engine's input up to another
//  system, such as a user interface library.  (You probably 
//  won't use this very much, if at all!)
// 
//  keyArray - pointer to a boolean array which will be
//             filled with the current keyboard state
//  size - the size of the boolean array (up to 256)
// 
//  Returns true if the size parameter was valid and false
//  if it was <= 0 or > 256
// ----------------------------------------------------------
bool Input::GetKeyArray(bool* keyArray, int size)
{
	if (size <= 0 || size > 256) return false;

	// Loop through the given size and fill the
	// boolean array.  Note that the double exclamation
	// point is on purpose; it's a quick way to
	// convert any number to a boolean.
	for (int i = 0; i < size; i++)
		keyArray[i] = !!(kbState[i] & 0x80);

	return true;
}


// ----------------------------------------------------------
//  Is the specific mouse button down this frame?
// ----------------------------------------------------------
bool Input::MouseLeftDown() { return (kbState[VK_LBUTTON] & 0x80) != 0; }
bool Input::MouseRightDown() { return (kbState[VK_RBUTTON] & 0x80) != 0; }
bool Input::MouseMiddleDown() { return (kbState[VK_MBUTTON] & 0x80) != 0; }


// ----------------------------------------------------------
//  Is the specific mouse button up this frame?
// ----------------------------------------------------------
bool Input::MouseLeftUp() { return !(kbState[VK_LBUTTON] & 0x80); }
bool Input::MouseRightUp() { return !(kbState[VK_RBUTTON] & 0x80); }
bool Input::MouseMiddleUp() { return !(kbState[VK_MBUTTON] & 0x80); }


// ----------------------------------------------------------
//  Was the specific mouse button initially 
// pressed or released this frame?
// ----------------------------------------------------------
bool Input::MouseLeftPress() { return kbState[VK_LBUTTON] & 0x80 && !(prevKbState[VK_LBUTTON] & 0x80); }
bool Input::MouseLeftRelease() { return !(kbState[VK_LBUTTON] & 0x80) && prevKbState[VK_LBUTTON] & 0x80; }

bool Input::MouseRightPress() { return kbState[VK_RBUTTON] & 0x80 && !(prevKbState[VK_RBUTTON] & 0x80); }
bool Input::MouseRightRelease() { return !(kbState[VK_RBUTTON] & 0x80) && prevKbState[VK_RBUTTON] & 0x80; }

bool Input::MouseMiddlePress() { return kbState[VK_MBUTTON] & 0x80 && !(prevKbState[VK_MBUTTON] & 0x80); }
bool Input::MouseMiddleRelease() { return !(kbState[VK_MBUTTON] & 0x80) && prevKbState[VK_MBUTTON] & 0x80; }
