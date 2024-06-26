#include "VKCore.h"
#include "Input.h"
#include "VulkanHelper.h"

#include <WindowsX.h>
#include <sstream>
#include <iostream>

// Define the static instance variable so our OS-level 
// message handling function below can talk to our object
VKCore* VKCore::VKCoreInstance = 0;

// --------------------------------------------------------
// The global callback function for handling windows OS-level messages.
//
// This needs to be a global function (not part of a class), but we want
// to forward the parameters to our class to properly handle them.
// --------------------------------------------------------
LRESULT VKCore::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return VKCoreInstance->ProcessMessage(hWnd, uMsg, wParam, lParam);
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
VKCore::VKCore(
	HINSTANCE hInstance,		// The application's handle
	const wchar_t* titleBarText,// Text for the window's title bar
	unsigned int windowWidth,	// Width of the window's client area
	unsigned int windowHeight,	// Height of the window's client area
	bool vsync,					// Sync the framerate to the monitor?
	bool debugTitleBarStats)	// Show extra stats (fps) in title bar?
	:
	hInstance(hInstance),
	titleBarText(titleBarText),
	windowWidth(windowWidth),
	windowHeight(windowHeight),
	vsync(vsync),
	isFullscreen(false),
	deviceSupportsTearing(false),
	titleBarStats(debugTitleBarStats),
	fpsTimeElapsed(0),
	fpsFrameCount(0),
	previousTime(0),
	currentTime(0),
	hasFocus(true),
	deltaTime(0),
	startTime(0),
	totalTime(0),
	hWnd(0),
	currentSwapBuffer(0),
	vkInstance(0),
	vkSurface(0),
	vkPhysicalDevice(0),
	vkDevice(0),
	vkGraphicsQueue(0),
	vkCommandPool(0),
	vkCommandBuffer(0),
	vkSwapchain(0),
	vkBackBufferImages{},
	vkBackBufferViews{},
	backBufferColorFormat(VK_FORMAT_B8G8R8A8_UNORM),
	viewport{},
	scissor{}
#if defined(DEBUG) || defined(_DEBUG)
	, debugMessenger(0)
#endif
{
	// Save a static reference to this object.
	//  - Since the OS-level message function must be a non-member (global) function, 
	//    it won't be able to directly interact with our VKCore object otherwise.
	//  - (Yes, a singleton might be a safer choice here).
	VKCoreInstance = this;

	// Query performance counter for accurate timing information
	__int64 perfFreq(0);
	QueryPerformanceFrequency((LARGE_INTEGER*)&perfFreq);
	perfCounterSeconds = 1.0 / (double)perfFreq;
}

// --------------------------------------------------------
// Destructor - Clean up (release) all Direct3D references
// --------------------------------------------------------
VKCore::~VKCore()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those Direct3D objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each Direct3D object created in VKCore

	// Clean up Vulkan
	for (unsigned int i = 0; i < numBackBuffers; i++)
	{
		vkDestroyImageView(vkDevice, vkBackBufferViews[i], 0);
	}
	vkDestroyCommandPool(vkDevice, vkCommandPool, 0);
	vkDestroySwapchainKHR(vkDevice, vkSwapchain, 0);
	vkDestroyDevice(vkDevice, 0);
	vkDestroySurfaceKHR(vkInstance, vkSurface, 0);
#if defined(DEBUG) || defined(_DEBUG)
	PFN_vkDestroyDebugUtilsMessengerEXT f = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
	if (f != 0) f(vkInstance, debugMessenger, 0);
#endif
	vkDestroyInstance(vkInstance, 0);

	// Delete singletons
	delete& Input::GetInstance();
	delete& VulkanHelper::GetInstance();
}

// --------------------------------------------------------
// Created the actual window for our application
// --------------------------------------------------------
HRESULT VKCore::InitWindow()
{
	// Start window creation by filling out the
	// appropriate window class struct
	WNDCLASS wndClass		= {}; // Zero out the memory
	wndClass.style			= CS_HREDRAW | CS_VREDRAW;	// Redraw on horizontal or vertical movement/adjustment
	wndClass.lpfnWndProc	= VKCore::WindowProc;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= 0;
	wndClass.hInstance		= hInstance;						// Our app's handle
	wndClass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);	// Default icon
	wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);		// Default arrow cursor
	wndClass.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= L"Direct3DWindowClass";

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

#if defined(DEBUG) || defined(_DEBUG)

static VKAPI_ATTR VkBool32 VKAPI_CALL ErrorCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* otherData
)
{
	printf("%s", callbackData->pMessage);
	printf("\n\n");
	return VK_FALSE;
}
#endif


// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Instance
// Dynamic rendering: https://lesleylai.info/en/vk-khr-dynamic-rendering/
VkResult VKCore::InitVulkan()
{

#if defined(DEBUG) || defined(_DEBUG)
	// --- DEBUG MESSENGER DETAILS ---------------------
	// This happens first because it is used in several
	// places, including the initial vulkan instance
	VkDebugUtilsMessengerCreateInfoEXT debugDesc = {};
	debugDesc.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugDesc.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugDesc.messageType =
		//VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugDesc.pfnUserCallback = ErrorCallback;
#endif

	// --- VULKAN INSTANCE -----------------------------

	// Describe the vulkan app
	VkApplicationInfo appDesc = {};
	appDesc.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appDesc.apiVersion = VK_API_VERSION_1_3;
	appDesc.applicationVersion = 0; // TODO: Does this matter?
	appDesc.engineVersion = 0; // TODO: Does this matter?
	appDesc.pApplicationName = "Test"; // TODO: Get this from param?
	appDesc.pEngineName = "Who cares?"; // TODO: Does this matter?

	// Extensions we want to load
	const char* extensionNames[] =
	{
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
		"VK_EXT_swapchain_colorspace"
#if defined(DEBUG) || defined(_DEBUG)
		, VK_EXT_DEBUG_UTILS_EXTENSION_NAME // Only in debug mode
#endif
	};
	uint32_t extensionCount = ARRAYSIZE(extensionNames);

	// Describe the vulkan instance we want, including extensions
	VkInstanceCreateInfo createDesc = {};
	createDesc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createDesc.pApplicationInfo = &appDesc;
	createDesc.enabledExtensionCount = extensionCount;
	createDesc.ppEnabledExtensionNames = extensionNames;
#if defined(DEBUG) || defined(_DEBUG)
	createDesc.pNext = &debugDesc;
#else
	createDesc.pNext = 0; // Null
#endif

	VK_TRY(vkCreateInstance(&createDesc, 0, &vkInstance));

	// --- WINDOW SURFACE ------------------------------------

	// Create window surface
	// TODO: Do we need this in addition to a swap chain?  Or just a swap chain?
	VkWin32SurfaceCreateInfoKHR surfaceDesc = {};
	surfaceDesc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceDesc.hinstance = this->hInstance;
	surfaceDesc.hwnd = this->hWnd;
	VK_TRY(vkCreateWin32SurfaceKHR(vkInstance, &surfaceDesc, 0, &vkSurface));

	// --- DEVICE ------------------------------

	// Choose proper device
	// Get the count
	unsigned int deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, 0);

	// Now make enough room for the list and grab it
	VkPhysicalDevice* devices = new VkPhysicalDevice[deviceCount];
	unsigned int deviceCountCopy = deviceCount; // Removes compiler warning about loop below
	vkEnumeratePhysicalDevices(vkInstance, &deviceCountCopy, devices);

	// Find the discrete card
	VkPhysicalDeviceProperties deviceProperties;
	for (unsigned int i = 0; i < deviceCount; i++)
	{
		vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			vkPhysicalDevice = devices[i];
			break;
		}
	}
	delete[] devices;

	// Anything?
	if (!vkPhysicalDevice)
		return VK_ERROR_UNKNOWN;;


	// --- QUEUE FAMILIES --------------------------------
	unsigned int queueFamCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamCount, 0);

	VkQueueFamilyProperties* queueFamProps = new VkQueueFamilyProperties[queueFamCount];
	unsigned int queueCountCopy = queueFamCount; // Removes compiler warning about loop below
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueCountCopy, queueFamProps);

	unsigned int graphicsQueueIndex = -1;
	for (unsigned int i = 0; i < queueFamCount; i++)
	{
		if (queueFamProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			graphicsQueueIndex = i;
			break;
		}
	}
	delete[] queueFamProps;

	// Did we find one?
	if (graphicsQueueIndex == -1)
		return VK_ERROR_UNKNOWN;



	// --- LOGICAL DEVICE --------------------------------

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceQueueCreateInfo queueDesc = {};
	queueDesc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueDesc.queueCount = 1;
	queueDesc.queueFamilyIndex = graphicsQueueIndex;

	const char* devExts[] = {
		"VK_KHR_swapchain",
		"VK_KHR_dynamic_rendering"
	};

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRendering = {};
	dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	dynamicRendering.dynamicRendering = VK_TRUE;

	VkDeviceCreateInfo deviceDesc = {};
	deviceDesc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceDesc.pNext = &dynamicRendering;
	deviceDesc.queueCreateInfoCount = 1;
	deviceDesc.pQueueCreateInfos = &queueDesc;
	deviceDesc.pEnabledFeatures = &deviceFeatures;
	deviceDesc.enabledExtensionCount = ARRAYSIZE(devExts);
	deviceDesc.ppEnabledExtensionNames = devExts;

#if defined(DEBUG) || defined(_DEBUG)
	const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
	deviceDesc.enabledLayerCount = ARRAYSIZE(layers);
	deviceDesc.ppEnabledLayerNames = layers;
#endif

	VK_TRY(vkCreateDevice(vkPhysicalDevice, &deviceDesc, 0, &vkDevice));

	// --- DEBUG MESSENGER -----------------------------
#if defined(DEBUG) || defined(_DEBUG)
	// Look up extension method for debug messenger creation
	PFN_vkCreateDebugUtilsMessengerEXT f = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
	if (f == 0) return VK_ERROR_EXTENSION_NOT_PRESENT;
	VK_TRY(f(vkInstance, &debugDesc, 0, &debugMessenger));

#endif


	// --- QUEUE HANDLE -----------------------------
	vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkGraphicsQueue);


	// --- SWAP CHAIN REQUIREMENTS ------------------

	// Grab number of formats, then fill the buffer
	//uint32_t formatCount = 0;
	//vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &formatCount, 0);
	//std::vector<VkSurfaceFormatKHR> formats(formatCount);
	//vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &formatCount, formats.data());


	// --- SWAP CHAIN -------------------------------
	VkSwapchainCreateInfoKHR swapchainDesc = {};
	swapchainDesc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainDesc.surface = vkSurface;
	swapchainDesc.minImageCount = numBackBuffers;
	swapchainDesc.imageArrayLayers = 1;
	swapchainDesc.imageColorSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT; // Could be VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainDesc.imageExtent.width = windowWidth;
	swapchainDesc.imageExtent.height = windowHeight;
	swapchainDesc.imageFormat = backBufferColorFormat;
	swapchainDesc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainDesc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainDesc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainDesc.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainDesc.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainDesc.clipped = VK_TRUE;

	VK_TRY(vkCreateSwapchainKHR(
		vkDevice,
		&swapchainDesc,
		0,
		&vkSwapchain));

	// --- GET SWAP CHAIN IMAGES --------------------------
	unsigned int swapCount = numBackBuffers;
	VK_TRY(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapCount, vkBackBufferImages));

	// --- CREATE SWAP CHAIN VIEWS -----------------------
	for (int i = 0; i < numBackBuffers; i++)
	{
		VkImageViewCreateInfo viewDesc = {};
		viewDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewDesc.image = vkBackBufferImages[i];
		viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewDesc.format = backBufferColorFormat;
		viewDesc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewDesc.subresourceRange.baseArrayLayer = 0;
		viewDesc.subresourceRange.layerCount = 1;
		viewDesc.subresourceRange.baseMipLevel = 0;
		viewDesc.subresourceRange.levelCount = 1;

		VK_TRY(vkCreateImageView(vkDevice, &viewDesc, 0, &vkBackBufferViews[i]));
	}

	// --- COMMAND POOL --------------------------------
	VkCommandPoolCreateInfo poolDesc = {};
	poolDesc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolDesc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolDesc.queueFamilyIndex = graphicsQueueIndex;
	
	VK_TRY(vkCreateCommandPool(vkDevice, &poolDesc, 0, &vkCommandPool));

	// --- COMMAND BUFFER ----------------------------
	VkCommandBufferAllocateInfo allocDesc = {};
	allocDesc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocDesc.commandPool = vkCommandPool;
	allocDesc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocDesc.commandBufferCount = 1;

	VK_TRY(vkAllocateCommandBuffers(vkDevice, &allocDesc, &vkCommandBuffer));

	// Initialize the helper
	VulkanHelper::GetInstance().Initialize(
		vkPhysicalDevice,
		vkDevice,
		vkCommandBuffer,
		vkGraphicsQueue,
		vkCommandPool);

	// --- VIEWPORT and SCISSOR ----------------------------
	viewport.x = 0.0f;
	viewport.y = (float)windowHeight; // Flipping upside down to match DX12?
	viewport.width = (float)windowWidth;
	viewport.height = -viewport.y; // Flipping upside down to match DX12?
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = windowWidth;
	scissor.extent.height = windowHeight;

	return VK_SUCCESS;
}

std::vector<VkLayerProperties> VKCore::GetLayerProperties(bool printNames)
{
	// Get the total first
	uint32_t totalLayerCount = 0;
	vkEnumerateInstanceLayerProperties(&totalLayerCount, 0);

	// Make enough room and query
	std::vector<VkLayerProperties> layerProperties(totalLayerCount);
	vkEnumerateInstanceLayerProperties(&totalLayerCount, layerProperties.data());

	// Should we print the names?
	if (printNames)
	{
		for (auto& l : layerProperties)
		{
			printf(l.layerName);
			printf("\n");
		}
	}

	return layerProperties;
}

std::vector<VkExtensionProperties> VKCore::GetInstanceExtensions(bool printNames)
{
	// Get the total first
	uint32_t totalExtCount = 0;
	vkEnumerateInstanceExtensionProperties(0, &totalExtCount, 0);

	// Make enough room and query
	std::vector<VkExtensionProperties> extensionProperties(totalExtCount);
	vkEnumerateInstanceExtensionProperties(0, &totalExtCount, extensionProperties.data());

	// Should we print the names?
	if (printNames)
	{
		for (auto& e : extensionProperties)
		{
			printf(e.extensionName);
			printf("\n");
		}
	}

	return extensionProperties;
}

std::vector<VkExtensionProperties> VKCore::GetDeviceExtensions(VkPhysicalDevice physicalDevice, const char* layerNameOrNull, bool printNames)
{
	// Get the total first
	uint32_t totalExtCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, layerNameOrNull, &totalExtCount, 0);

	// Make enough room and query
	std::vector<VkExtensionProperties> extensionProperties(totalExtCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, layerNameOrNull, &totalExtCount, extensionProperties.data());

	// Should we print the names?
	if (printNames)
	{
		for (auto& e : extensionProperties)
		{
			printf(e.extensionName);
			printf("\n");
		}
	}

	return extensionProperties;
}



// --------------------------------------------------------
// When the window is resized, the underlying 
// buffers (textures) must also be resized to match.
//
// If we don't do this, the window size and our rendering
// resolution won't match up.  This can result in odd
// stretching/skewing.
// --------------------------------------------------------
void VKCore::OnResize()
{
	// Wait for the GPU
	vkDeviceWaitIdle(vkDevice);

	// Remove the existing swap chain
	for (unsigned int i = 0; i < numBackBuffers; i++)
		vkDestroyImageView(vkDevice, vkBackBufferViews[i], 0);
	vkDestroySwapchainKHR(vkDevice, vkSwapchain, 0);

	// --- SWAP CHAIN -------------------------------
	VkSwapchainCreateInfoKHR swapchainDesc = {};
	swapchainDesc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainDesc.surface = vkSurface;
	swapchainDesc.minImageCount = numBackBuffers;
	swapchainDesc.imageArrayLayers = 1;
	swapchainDesc.imageColorSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT; // Could be VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainDesc.imageExtent.width = windowWidth;
	swapchainDesc.imageExtent.height = windowHeight;
	swapchainDesc.imageFormat = backBufferColorFormat;
	swapchainDesc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainDesc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainDesc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainDesc.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainDesc.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainDesc.clipped = VK_TRUE;

	vkCreateSwapchainKHR(
		vkDevice,
		&swapchainDesc,
		0,
		&vkSwapchain);

	// --- GET SWAP CHAIN IMAGES --------------------------
	unsigned int swapCount = numBackBuffers;
	vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapCount, vkBackBufferImages);

	// --- CREATE SWAP CHAIN VIEWS -----------------------
	for (int i = 0; i < numBackBuffers; i++)
	{
		VkImageViewCreateInfo viewDesc = {};
		viewDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewDesc.image = vkBackBufferImages[i];
		viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewDesc.format = backBufferColorFormat;
		viewDesc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // Zero
		viewDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewDesc.subresourceRange.baseArrayLayer = 0;
		viewDesc.subresourceRange.layerCount = 1;
		viewDesc.subresourceRange.baseMipLevel = 0;
		viewDesc.subresourceRange.levelCount = 1;

		vkCreateImageView(vkDevice, &viewDesc, 0, &vkBackBufferViews[i]);
	}

	// --- UPDATE VIEWPORT & SCISSOR ---------------------
	viewport.x = 0.0f;
	viewport.y = (float)windowHeight; // Flipping upside down to match DX12?
	viewport.width = (float)windowWidth;
	viewport.height = -viewport.y; // Flipping upside down to match DX12?
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = windowWidth;
	scissor.extent.height = windowHeight;
}


// --------------------------------------------------------
// This is the main game loop, handling the following:
//  - OS-level messages coming in from Windows itself
//  - Calling update & draw back and forth, forever
// --------------------------------------------------------
HRESULT VKCore::Run()
{
	// Grab the start time now that
	// the game loop is running
	__int64 now(0);
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
void VKCore::Quit()
{
	PostMessage(this->hWnd, WM_CLOSE, NULL, NULL);
}


// --------------------------------------------------------
// Uses high resolution time stamps to get very accurate
// timing information, and calculates useful time stats
// --------------------------------------------------------
void VKCore::UpdateTimer()
{
	// Grab the current time
	__int64 now(0);
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
void VKCore::UpdateTitleBarStats()
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

	// Append the version of Vulkan
	output << "    Vulkan";

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
void VKCore::CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
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
LRESULT VKCore::ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

		// If initialized, resize our required buffers
		if (vkDevice) 
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

