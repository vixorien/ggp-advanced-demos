#pragma once

#include <vulkan/vulkan.h>

#define VK_TRY(x) { VkResult r = x; if(r != VK_SUCCESS) return r; }

class VulkanHelper
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static VulkanHelper& GetInstance()
	{
		if (!instance)
		{
			instance = new VulkanHelper();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	VulkanHelper(VulkanHelper const&) = delete;
	void operator=(VulkanHelper const&) = delete;

private:
	static VulkanHelper* instance;
	VulkanHelper() :
		vkPhysicalDevice(0),
		vkDevice(0),
		vkCommandBuffer(0),
		vkGraphicsQueue(0),
		vkCommandPool(0)
	{ };
#pragma endregion

public:
	~VulkanHelper();

	// Initialization for singleton
	void Initialize(
		VkPhysicalDevice vkPhysicalDevice,
		VkDevice vkDevice,
		VkCommandBuffer vkCommandBuffer,
		VkQueue vkGraphicsQueue,
		VkCommandPool vkCommandPool
		//Microsoft::WRL::ComPtr<ID3D12Device> device,
		//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		//Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		//Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator
	);

	// Resource creation
	uint32_t GetMemoryType(
		VkMemoryRequirements memRequirements,
		VkMemoryPropertyFlags memProperties);
	
	VkResult CreateStaticBuffer(
		uint64_t dataStride,
		uint64_t dataCount,
		void* data,
		VkBufferUsageFlags bufferUsage, 
		VkBuffer& buffer,
		VkDeviceMemory& bufferMemory);

	// Command list & synchronization
	void CloseExecuteAndResetCommandList();
	void WaitForGPU();


private:

	VkPhysicalDevice vkPhysicalDevice;
	VkDevice vkDevice;
	VkCommandBuffer vkCommandBuffer;
	VkQueue vkGraphicsQueue;
	VkCommandPool vkCommandPool;
	//// Overall device
	//Microsoft::WRL::ComPtr<ID3D12Device> device;

	//// Command list related
	//// Note: We're assuming a single command list for the entire
	//// engine at this point.  That's not always true for more
	//// complex engines but should be fine for us
	//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList;
	//Microsoft::WRL::ComPtr<ID3D12CommandQueue>			commandQueue;
	//Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		commandAllocator;

	//// Basic CPU/GPU synchronization
	//Microsoft::WRL::ComPtr<ID3D12Fence> waitFence;
	//HANDLE								waitFenceEvent;
	//unsigned long						waitFenceCounter;
};

