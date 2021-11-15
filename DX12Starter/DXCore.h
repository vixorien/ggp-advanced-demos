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
		const char* titleBarText,	// Text for the window's title bar
		unsigned int windowWidth,	// Width of the window's client area
		unsigned int windowHeight,	// Height of the window's client area
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
	HRESULT InitDirectX();
	HRESULT Run();
	void Quit();
	virtual void OnResize();

	// Pure virtual methods for setup and game functionality
	virtual void Init() = 0;
	virtual void Update(float deltaTime, float totalTime) = 0;
	virtual void Draw(float deltaTime, float totalTime) = 0;

protected:
	HINSTANCE	hInstance;		// The handle to the application
	HWND		hWnd;			// The handle to the window itself
	std::string titleBarText;	// Custom text in window's title bar
	bool		titleBarStats;	// Show extra stats in title bar?

	// Size of the window's client area
	unsigned int width;
	unsigned int height;

	// Does our window currently have focus?
	// Helpful if we want to pause while not the active window
	bool hasFocus;

	// Swap chain buffer tracking
	static const unsigned int numBackBuffers = 2;
	unsigned int currentSwapBuffer;

	// DirectX related objects and variables
	D3D_FEATURE_LEVEL		dxFeatureLevel;
	Microsoft::WRL::ComPtr<ID3D12Device>		device;
	Microsoft::WRL::ComPtr<IDXGISwapChain>		swapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		commandAllocator;
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

	// Fence for CPU/GPU sync
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	unsigned long currentFence = 0;

	// DX12 Helper Functions
	void WaitForGPU();
	void CloseExecuteAndResetCommandList();

	// Helper function for allocating a console window
	void CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns);

	// Helpers for determining the actual path to the executable
	std::string GetExePath();
	std::wstring GetExePath_Wide();

	std::string GetFullPathTo(std::string relativeFilePath);
	std::wstring GetFullPathTo_Wide(std::wstring relativeFilePath);

	// DX12:

	// bool vsync (helper)
	// Helpers for...
	// - CreateStaticBuffer() for making things like index and vertex buffers?
	// - CreateIndexBuffer()
	// - CreateVertexBuffer()
	// Potentially handle gpu sync frames
	// - unsigned int gpuSyncFrames (about 3 or so)
	// 	HANDLE			syncFenceEvent;
	//  UINT64			syncFenceCounter;
	//  ID3D12Fence* syncFence;
	//


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

