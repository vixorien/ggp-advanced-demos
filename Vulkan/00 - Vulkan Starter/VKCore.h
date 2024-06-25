#pragma once

// Must have define BEFORE include!
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>


class VKCore
{
public:
	VKCore(
		HINSTANCE hInstance,		// The application's handle
		const wchar_t* titleBarText,// Text for the window's title bar
		unsigned int windowWidth,	// Width of the window's client area
		unsigned int windowHeight,	// Height of the window's client area
		bool vsync,					// Sync the framerate to the monitor?
		bool debugTitleBarStats);	// Show extra stats (fps) in title bar?
	~VKCore();

	// Static requirements for OS-level message processing
	static VKCore* VKCoreInstance;
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
	VkResult InitVulkan();
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
	static const unsigned int numBackBuffers = 2;
	unsigned int currentSwapBuffer;

	// Vulkan related objects and variables
	VkInstance vkInstance;
	VkSurfaceKHR vkSurface;
	VkPhysicalDevice vkPhysicalDevice;
	VkDevice vkDevice;
	VkSwapchainKHR vkSwapchain;

	VkQueue vkGraphicsQueue;
	VkCommandPool vkCommandPool;
	VkCommandBuffer vkCommandBuffer;

	VkFormat backBufferColorFormat;
	VkImage vkBackBufferImages[numBackBuffers];
	VkImageView vkBackBufferViews[numBackBuffers];

	// Vulkan extensions and validation layer checks
	std::vector<VkLayerProperties> GetLayerProperties(bool printNames);
	std::vector<VkExtensionProperties> GetInstanceExtensions(bool printNames);
	std::vector<VkExtensionProperties> GetDeviceExtensions(VkPhysicalDevice physicalDevice, const char* layerNameOrNull, bool printNames);

	// Vulkan error callbacks (for debug mode)
#if defined(DEBUG) || defined(_DEBUG)
	VkDebugUtilsMessengerEXT debugMessenger;
#endif

	// DirectX related objects and variables
	//D3D_FEATURE_LEVEL		dxFeatureLevel;
	//Microsoft::WRL::ComPtr<ID3D12Device>		device;
	//Microsoft::WRL::ComPtr<IDXGISwapChain>		swapChain;

	//Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		commandAllocator;
	//Microsoft::WRL::ComPtr<ID3D12CommandQueue>			commandQueue; 
	//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList;
	//
	//size_t rtvDescriptorSize;
	//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[numBackBuffers]; // Pointers into the RTV desc heap
	//D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	//
	//Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[numBackBuffers];
	//Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;

	//D3D12_VIEWPORT			viewport;
	//D3D12_RECT				scissorRect;

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

