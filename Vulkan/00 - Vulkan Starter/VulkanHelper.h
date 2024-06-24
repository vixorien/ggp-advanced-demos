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

	//// Basic CPU/GPU synchronization
	//Microsoft::WRL::ComPtr<ID3D12Fence> waitFence;
	//HANDLE								waitFenceEvent;
	//unsigned long						waitFenceCounter;
};

