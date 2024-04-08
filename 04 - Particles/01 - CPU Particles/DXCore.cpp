#include "DXCore.h"
#include "Input.h"

#include <dxgi1_5.h>
#include <WindowsX.h>
#include <sstream>

// Define the static instance variable so our OS-level 
// message handling function below can talk to our object
DXCore* DXCore::DXCoreInstance = 0;

// --------------------------------------------------------
// The global callback function for handling windows OS-level messages.
//
// This needs to be a global function (not part of a class), but we want
// to forward the parameters to our class to properly handle them.
// --------------------------------------------------------
LRESULT DXCore::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DXCoreInstance->ProcessMessage(hWnd, uMsg, wParam, lParam);
}

// --------------------------------------------------------
// Constructor - Set up fields and timer
//
// hInstance	- The application's OS-level handle (unique ID)
// titleBarText - Text for the window's title bar
// windowWidth	- Width of the window's client (internal) area
// windowHeight - Height of the window's client (internal) area
// debugTitleBarStats - Show debug stats in the title bar, like FPS?
// --------------------------------------------------------
DXCore::DXCore(
	HINSTANCE hInstance,		// The application's handle
	const wchar_t* titleBarText,// Text for the window's title bar
	unsigned int windowWidth,	// Width of the window's client area
	unsigned int windowHeight,	// Height of the window's client area
	bool vsync,					// Sync the framerate to the monitor?
	bool debugTitleBarStats) 	// Show extra stats (fps) in title bar?
	:
	hInstance(hInstance),
	titleBarText(titleBarText),
	windowWidth(windowWidth),
	windowHeight(windowHeight),
	vsync(vsync),
	isFullscreen(false),
	deviceSupportsTearing(false),
	titleBarStats(debugTitleBarStats),
	dxFeatureLevel(D3D_FEATURE_LEVEL_11_0),
	fpsTimeElapsed(0),
	fpsFrameCount(0),
	previousTime(0),
	currentTime(0),
	hasFocus(true),
	deltaTime(0),
	startTime(0),
	totalTime(0),
	hWnd(0)
{
	// Save a static reference to this object.
	//  - Since the OS-level message function must be a non-member (global) function, 
	//    it won't be able to directly interact with our DXCore object otherwise.
	//  - (Yes, a singleton might be a safer choice here).
	DXCoreInstance = this;

	// Query performance counter for accurate timing information
	__int64 perfFreq = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&perfFreq);
	perfCounterSeconds = 1.0 / (double)perfFreq;
}

// --------------------------------------------------------
// Destructor - Clean up (release) all Direct3D references
// --------------------------------------------------------
DXCore::~DXCore()
{
	// Note: Since we're using ComPtrs for Direct3D resources,
	//  we don't need to explicitly clean them up here
	// - If we weren't using smart pointers, we'd need to call
	//   Release() on each Direct3D object created in DXCore

	// Delete input manager singleton
	delete& Input::GetInstance();
}

// --------------------------------------------------------
// Creates the actual window for our application
// --------------------------------------------------------
HRESULT DXCore::InitWindow()
{
	// Start window creation by filling out the
	// appropriate window class struct
	WNDCLASS wndClass		= {}; // Zero out the memory
	wndClass.style			= CS_HREDRAW | CS_VREDRAW;	// Redraw on horizontal or vertical movement/adjustment
	wndClass.lpfnWndProc	= DXCore::WindowProc;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= 0;
	wndClass.hInstance		= hInstance;						// Our app's handle
	wndClass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);	// Default icon
	wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);		// Default arrow cursor
	wndClass.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= L"Direct3DWindowClass"; // The "L" means this is a wide-character string

	// Attempt to register the window class we've defined
	if (!RegisterClass(&wndClass))
	{
		// Get the most recent error
		DWORD error = GetLastError();

		// If the class exists, that's actually fine.  Otherwise,
		// we can't proceed with the next step.
		if (error != ERROR_CLASS_ALREADY_EXISTS)
			return HRESULT_FROM_WIN32(error);
	}

	// Adjust the width and height so the "client size" matches
	// the width and height given (the inner-area of the window)
	RECT clientRect;
	SetRect(&clientRect, 0, 0, windowWidth, windowHeight);
	AdjustWindowRect(
		&clientRect,
		WS_OVERLAPPEDWINDOW,	// Has a title bar, border, min and max buttons, etc.
		false);					// No menu bar

	// Center the window to the screen
	RECT desktopRect;
	GetClientRect(GetDesktopWindow(), &desktopRect);
	int centeredX = (desktopRect.right / 2) - (clientRect.right / 2);
	int centeredY = (desktopRect.bottom / 2) - (clientRect.bottom / 2);

	// Actually ask Windows to create the window itself
	// using our settings so far.  This will return the
	// handle of the window, which we'll keep around for later
	hWnd = CreateWindow(
		wndClass.lpszClassName,
		titleBarText.c_str(),
		WS_OVERLAPPEDWINDOW,
		centeredX,
		centeredY,
		clientRect.right - clientRect.left,	// Calculated width
		clientRect.bottom - clientRect.top,	// Calculated height
		0,			// No parent window
		0,			// No menu
		hInstance,	// The app's handle
		0);			// No other windows in our application

	// Ensure the window was created properly
	if (hWnd == NULL)
	{
		DWORD error = GetLastError();
		return HRESULT_FROM_WIN32(error);
	}

	// The window exists but is not visible yet
	// We need to tell Windows to show it, and how to show it
	ShowWindow(hWnd, SW_SHOW);

	// Initialize the input manager now that we definitely have a window
	Input::GetInstance().Initialize(hWnd);

	// Return an "everything is ok" HRESULT value
	return S_OK;
}


// --------------------------------------------------------
// Initializes Direct3D, which requires a window.  This method
// also creates several common Direct3D objects we'll need to 
// start drawing things to the screen.
// --------------------------------------------------------
HRESULT DXCore::InitDirect3D()
{
	// This will hold options for Direct3D initialization
	unsigned int deviceFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
	// If we're in debug mode in visual studio, we also
	// want to make a "Debug Direct3D Device" to see some
	// errors and warnings in Visual Studio's output window
	// when things go wrong!
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Determine if screen tearing ("vsync off") is available
	// - This is necessary due to variable refresh rate displays
	Microsoft::WRL::ComPtr<IDXGIFactory5> factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
	{
		// Check for this specific feature (must use BOOL typedef here!)
		BOOL tearingSupported = false;
		HRESULT featureCheck = factory->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&tearingSupported,
			sizeof(tearingSupported));

		// Final determination of support
		deviceSupportsTearing = SUCCEEDED(featureCheck) && tearingSupported;
	}

	// Create a description of how our swap
	// chain should work
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount		= 2;
	swapDesc.BufferDesc.Width	= windowWidth;
	swapDesc.BufferDesc.Height	= windowHeight;
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.BufferDesc.Format	= DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapDesc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.Flags				= deviceSupportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	swapDesc.OutputWindow		= hWnd;
	swapDesc.SampleDesc.Count	= 1;
	swapDesc.SampleDesc.Quality = 0;
	swapDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.Windowed			= true;

	// Result variable for below function calls
	HRESULT hr = S_OK;

	// Attempt to initialize Direct3D
	hr = D3D11CreateDeviceAndSwapChain(
		0,							// Video adapter (physical GPU) to use, or null for default
		D3D_DRIVER_TYPE_HARDWARE,	// We want to use the hardware (GPU)
		0,							// Used when doing software rendering
		deviceFlags,				// Any special options
		0,							// Optional array of possible verisons we want as fallbacks
		0,							// The number of fallbacks in the above param
		D3D11_SDK_VERSION,			// Current version of the SDK
		&swapDesc,					// Address of swap chain options
		swapChain.GetAddressOf(),	// Pointer to our Swap Chain pointer
		device.GetAddressOf(),		// Pointer to our Device pointer
		&dxFeatureLevel,			// This will hold the actual feature level the app will use
		context.GetAddressOf());	// Pointer to our Device Context pointer
	if (FAILED(hr)) return hr;

	// Create the Render Target View for the back buffer render target
	{
		// The above function created the back buffer texture for us
		// but we need to get a reference to it for the next step
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBufferTexture;
		swapChain->GetBuffer(
			0,
			__uuidof(ID3D11Texture2D),
			(void**)backBufferTexture.GetAddressOf());

		// Now that we have the texture ref, create a render target view
		// for the back buffer so we can render into it.
		if (backBufferTexture != 0)
		{
			device->CreateRenderTargetView(backBufferTexture.Get(),	0, backBufferRTV.GetAddressOf());
		}
	}

	// Create the Depth Buffer and associated Depth Stencil View
	{
		// Set up the description of the texture to use for the depth buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc	= {};
		depthStencilDesc.Width					= windowWidth;
		depthStencilDesc.Height					= windowHeight;
		depthStencilDesc.MipLevels				= 1;
		depthStencilDesc.ArraySize				= 1;
		depthStencilDesc.Format					= DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.Usage					= D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags				= D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags			= 0;
		depthStencilDesc.MiscFlags				= 0;
		depthStencilDesc.SampleDesc.Count		= 1;
		depthStencilDesc.SampleDesc.Quality		= 0;

		// Create the depth buffer texture resource
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBufferTexture;
		device->CreateTexture2D(&depthStencilDesc, 0, depthBufferTexture.GetAddressOf());

		// As long as the depth buffer texture was created successfully, 
		// create the associated Depth Stencil View so we can use it for rendering
		if (depthBufferTexture != 0)
		{
			device->CreateDepthStencilView(depthBufferTexture.Get(), 0,	depthBufferDSV.GetAddressOf());
		}
	}

	// Bind the back buffer and depth buffer to the pipeline
	// so these particular resources are used when rendering
	context->OMSetRenderTargets(
		1, 
		backBufferRTV.GetAddressOf(), 
		depthBufferDSV.Get());

	// Lastly, set up a viewport so we render into
	// to correct portion of the window
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX	= 0;
	viewport.TopLeftY	= 0;
	viewport.Width		= (float)windowWidth;
	viewport.Height		= (float)windowHeight;
	viewport.MinDepth	= 0.0f;
	viewport.MaxDepth	= 1.0f;
	context->RSSetViewports(1, &viewport);

	// Return the "everything is ok" HRESULT value
	return S_OK;
}

// --------------------------------------------------------
// When the window is resized, the underlying 
// buffers (textures) must also be resized to match.
//
// If we don't do this, the window size and our rendering
// resolution won't match up.  This can result in odd
// stretching/skewing.
// --------------------------------------------------------
void DXCore::OnResize()
{
	// Resize the buffers that must match the window size
	{
		// Release the views before resizing the swap chain,
		// as there cannot be any outstanding references to
		// the back buffer before the resize operation
		backBufferRTV.Reset();
		depthBufferDSV.Reset();

		// Resize the underlying swap chain buffers,
		// which essentially destroys and recreates them
		swapChain->ResizeBuffers(
			2,
			windowWidth,
			windowHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			deviceSupportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
	}

	// A new back buffer requires a new Render Target View
	{
		// Get the texture reference
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBufferTexture;
		swapChain->GetBuffer(0,	__uuidof(ID3D11Texture2D), (void**)backBufferTexture.GetAddressOf());

		// Recreate the Render Target View for the back buffer texture
		if (backBufferTexture != 0)
		{
			device->CreateRenderTargetView(backBufferTexture.Get(),	0, backBufferRTV.GetAddressOf());
		}
	}

	// Since the window size changed, we need a new depth buffer too!
	{
		// Set up the description of the texture to use for the depth buffer
		D3D11_TEXTURE2D_DESC depthStencilDesc = {};
		depthStencilDesc.Width = windowWidth;
		depthStencilDesc.Height = windowHeight;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;

		// Create the depth buffer texture resource
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBufferTexture;
		device->CreateTexture2D(&depthStencilDesc, 0, depthBufferTexture.GetAddressOf());

		// As long as the depth buffer texture was created successfully, 
		// create the associated Depth Stencil View so we can use it for rendering
		if (depthBufferTexture != 0)
		{
			device->CreateDepthStencilView(depthBufferTexture.Get(), 0,	depthBufferDSV.GetAddressOf());
		}
	}

	// Bind the back buffer and depth buffer to the pipeline
	// so these particular resources are used when rendering
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());

	// Set up a viewport so we render into
	// to correct portion of the window
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX	= 0;
	viewport.TopLeftY	= 0;
	viewport.Width		= (float)windowWidth;
	viewport.Height		= (float)windowHeight;
	viewport.MinDepth	= 0.0f;
	viewport.MaxDepth	= 1.0f;
	context->RSSetViewports(1, &viewport);

	// Are we in a fullscreen state?
 	swapChain->GetFullscreenState(&isFullscreen, 0);
}


// --------------------------------------------------------
// This is the main game loop, handling the following:
//  - OS-level messages coming in from Windows itself
//  - Calling update & draw back and forth, forever
// --------------------------------------------------------
HRESULT DXCore::Run()
{
	// Grab the start time now that
	// the game loop is running
	__int64 now = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	startTime = now;
	currentTime = now;
	previousTime = now;

	// Give subclass a chance to initialize
	Init();

	// Our overall game and message loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Determine if there is a message waiting
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate and dispatch the message
			// to our custom WindowProc function
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Update timer and title bar (if necessary)
			UpdateTimer();
			if(titleBarStats)
				UpdateTitleBarStats();

			// Update the input manager
			Input::GetInstance().Update();

			// The game loop
			Update(deltaTime, totalTime);
			Draw(deltaTime, totalTime);

			// Frame is over, notify the input manager
			Input::GetInstance().EndOfFrame();
		}
	}

	// We'll end up here once we get a WM_QUIT message,
	// which usually comes from the user closing the window
	return (HRESULT)msg.wParam;
}


// --------------------------------------------------------
// Sends an OS-level window close message to our process, which
// will be handled by our message processing function
// --------------------------------------------------------
void DXCore::Quit()
{
	PostMessage(this->hWnd, WM_CLOSE, NULL, NULL);
}


// --------------------------------------------------------
// Uses high resolution time stamps to get very accurate
// timing information, and calculates useful time stats
// --------------------------------------------------------
void DXCore::UpdateTimer()
{
	// Grab the current time
	__int64 now = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	currentTime = now;

	// Calculate delta time and clamp to zero
	//  - Could go negative if CPU goes into power save mode 
	//    or the process itself gets moved to another core
	deltaTime = max((float)((currentTime - previousTime) * perfCounterSeconds), 0.0f);

	// Calculate the total time from start to now
	totalTime = (float)((currentTime - startTime) * perfCounterSeconds);

	// Save current time for next frame
	previousTime = currentTime;
}


// --------------------------------------------------------
// Updates the window's title bar with several stats once
// per second, including:
//  - The window's width & height
//  - The current FPS and ms/frame
//  - The version of Direct3D actually being used (usually 11)
// --------------------------------------------------------
void DXCore::UpdateTitleBarStats()
{
	fpsFrameCount++;

	// Only calc FPS and update title bar once per second
	float timeDiff = totalTime - fpsTimeElapsed;
	if (timeDiff < 1.0f)
		return;

	// How long did each frame take?  (Approx)
	float mspf = 1000.0f / (float)fpsFrameCount;

	// Quick and dirty title bar text (mostly for debugging)
	std::wostringstream output;
	output.precision(6);
	output << titleBarText <<
		"    Width: "		<< windowWidth <<
		"    Height: "		<< windowHeight <<
		"    FPS: "			<< fpsFrameCount <<
		"    Frame Time: "	<< mspf << "ms";

	// Append the version of Direct3D the app is using
	switch (dxFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_11_1: output << "    D3D 11.1"; break;
	case D3D_FEATURE_LEVEL_11_0: output << "    D3D 11.0"; break;
	case D3D_FEATURE_LEVEL_10_1: output << "    D3D 10.1"; break;
	case D3D_FEATURE_LEVEL_10_0: output << "    D3D 10.0"; break;
	case D3D_FEATURE_LEVEL_9_3:  output << "    D3D 9.3";  break;
	case D3D_FEATURE_LEVEL_9_2:  output << "    D3D 9.2";  break;
	case D3D_FEATURE_LEVEL_9_1:  output << "    D3D 9.1";  break;
	default:                     output << "    D3D ???";  break;
	}

	// Actually update the title bar and reset fps data
	SetWindowText(hWnd, output.str().c_str());
	fpsFrameCount = 0;
	fpsTimeElapsed += 1.0f;
}

// --------------------------------------------------------
// Allocates a console window we can print to for debugging
// 
// bufferLines   - Number of lines in the overall console buffer
// bufferColumns - Numbers of columns in the overall console buffer
// windowLines   - Number of lines visible at once in the window
// windowColumns - Number of columns visible at once in the window
// --------------------------------------------------------
void DXCore::CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
{
	// Our temp console info struct
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// Get the console info and set the number of lines
	AllocConsole();
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = bufferLines;
	coninfo.dwSize.X = bufferColumns;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	SMALL_RECT rect = {};
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = windowColumns;
	rect.Bottom = windowLines;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);

	FILE *stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stdout);
	freopen_s(&stream, "CONOUT$", "w", stderr);

	// Prevent accidental console window close
	HWND consoleHandle = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(consoleHandle, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
}



// --------------------------------------------------------
// Handles messages that are sent to our window by the
// operating system.  Ignoring these messages would cause
// our program to hang and Windows would think it was
// unresponsive.
// --------------------------------------------------------
LRESULT DXCore::ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Check the incoming message and handle any we care about
	switch (uMsg)
	{
	// This is the message that signifies the window closing
	case WM_DESTROY:
		PostQuitMessage(0); // Send a quit message to our own program
		return 0;

	// Prevent beeping when we "alt-enter" into fullscreen
	case WM_MENUCHAR: 
		return MAKELRESULT(0, MNC_CLOSE);

	// Prevent the overall window from becoming too small
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	// Sent when the window size changes
	case WM_SIZE:
		// Don't adjust anything when minimizing,
		// since we end up with a width/height of zero
		// and that doesn't play well with the GPU
		if (wParam == SIZE_MINIMIZED)
			return 0;
		
		// Save the new client area dimensions.
		windowWidth = LOWORD(lParam);
		windowHeight = HIWORD(lParam);

		// If DX is initialized, resize 
		// our required buffers
		if (device) 
			OnResize();

		return 0;

	// Has the mouse wheel been scrolled?
	case WM_MOUSEWHEEL:
		Input::GetInstance().SetWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
		return 0;

	// Raw mouse input
	case WM_INPUT:
		Input::GetInstance().ProcessRawMouseInput(lParam);
		break;
	
	// Is our focus state changing?
	case WM_SETFOCUS:	hasFocus = true;	return 0;
	case WM_KILLFOCUS:	hasFocus = false;	return 0;
	case WM_ACTIVATE:	hasFocus = (LOWORD(wParam) != WA_INACTIVE); return 0;
	}

	// Let Windows handle any messages we're not touching
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

