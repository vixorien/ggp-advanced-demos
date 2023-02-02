#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects

// We can include the correct library files here
// instead of in Visual Studio settings if we want
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DXCore
{
public:
	DXCore(
		HINSTANCE hInstance,		// The application's handle
		const wchar_t* titleBarText,// Text for the window's title bar
		unsigned int windowWidth,	// Width of the window's client area
		unsigned int windowHeight,	// Height of the window's client area
		bool vsync,					// Sync the framerate to the monitor?
		bool debugTitleBarStats);	// Show extra stats (fps) in title bar?
	~DXCore();

	// Static requirements for OS-level message processing
	static DXCore* DXCoreInstance;
	static LRESULT CALLBACK WindowProc(
		HWND hWnd,		// Window handle
		UINT uMsg,		// Message
		WPARAM wParam,	// Message's first parameter
		LPARAM lParam	// Message's second parameter
	);

	// Internal method for message handling
	LRESULT ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Initialization and game-loop related methods
	HRESULT InitWindow();
	HRESULT InitDirect3D();
	HRESULT Run();
	void Quit();
	virtual void OnResize();

	// Pure virtual methods for setup and game functionality
	virtual void Init() = 0;
	virtual void Update(float deltaTime, float totalTime) = 0;
	virtual void Draw(float deltaTime, float totalTime) = 0;

protected:
	HINSTANCE		hInstance;		// The handle to the application
	HWND			hWnd;			// The handle to the window itself
	std::wstring	titleBarText;	// Custom text in window's title bar
	bool			titleBarStats;	// Show extra stats in title bar?

	// Size of the window's client area
	unsigned int windowWidth;
	unsigned int windowHeight;

	// Does our window currently have focus?
	// Helpful if we want to pause while not the active window
	bool hasFocus;

	// Should our framerate sync to the vertical refresh
	// of the monitor (true) or run as fast as possible (false)?
	bool vsync;
	bool deviceSupportsTearing;
	BOOL isFullscreen; // Due to alt+enter key combination (must be BOOL typedef)

	// Swap chain buffer tracking
	static const unsigned int numBackBuffers = 3;
	unsigned int currentSwapBuffer;

	// DirectX related objects and variables
	D3D_FEATURE_LEVEL		dxFeatureLevel;
	Microsoft::WRL::ComPtr<ID3D12Device>		device;
	Microsoft::WRL::ComPtr<IDXGISwapChain>		swapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		commandAllocators[numBackBuffers];
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			commandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList;

	unsigned int rtvDescriptorSize;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[numBackBuffers]; // Pointers into the RTV desc heap
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;

	Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[numBackBuffers];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;

	D3D12_VIEWPORT			viewport;
	D3D12_RECT				scissorRect;

	// Helper function for allocating a console window
	void CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns);


private:
	// Timing related data
	double perfCounterSeconds;
	float totalTime;
	float deltaTime;
	__int64 startTime;
	__int64 currentTime;
	__int64 previousTime;

	// FPS calculation
	int fpsFrameCount;
	float fpsTimeElapsed;

	void UpdateTimer();			// Updates the timer for this frame
	void UpdateTitleBarStats();	// Puts debug info in the title bar
};

