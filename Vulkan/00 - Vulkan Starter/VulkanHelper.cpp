#include "VulkanHelper.h"
#include <cstring> // for memcpy

// Singleton requirement
VulkanHelper* VulkanHelper::instance;


// --------------------------------------------------------
// Destructor doesn't have much to do since we're using
// ComPtrs for all DX12 objects
// --------------------------------------------------------
VulkanHelper::~VulkanHelper() { }


// --------------------------------------------------------
// Sets up the helper with required DX12 objects
// --------------------------------------------------------
void VulkanHelper::Initialize(
	VkPhysicalDevice vkPhysicalDevice,
	VkDevice vkDevice,
	VkCommandBuffer vkCommandBuffer,
	VkQueue vkGraphicsQueue,
	VkCommandPool vkCommandPool)
{
	this->vkPhysicalDevice = vkPhysicalDevice;
	this->vkDevice = vkDevice;
	this->vkCommandBuffer = vkCommandBuffer;
	this->vkGraphicsQueue = vkGraphicsQueue;
	this->vkCommandPool = vkCommandPool;
}

// -------------------------------------------------------
// Helper to look up the index for the available memory
// types, based on requirements and desired properties
// 
// memRequirements - What kind of memory is required?
// memFlags - How do we want the memory to work?
// --------------------------------------------------------
uint32_t VulkanHelper::GetMemoryType(VkMemoryRequirements memRequirements, VkMemoryPropertyFlags memFlags)
{
	// Grab the properties of memory on the device
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memProperties);

	unsigned int memTypeIndex = -1;
	for (unsigned int i = 0; i < memProperties.memoryTypeCount; i++)
	{
		// Validate memory type from requirements
		if (!(memRequirements.memoryTypeBits & (1 << i))) continue;

		// Validate memory flags to get proper type of memory
		if (!((memProperties.memoryTypes[i].propertyFlags & memFlags) == memFlags)) continue;

		// Success
		return i;
	}

	return -1;
}

// --------------------------------------------------------
// Helper for creating a static buffer that will get
// data once and remain immutable
// 
// dataStride - The size of one piece of data in the buffer (like a vertex)
// dataCount - How many pieces of data (like how many vertices)
// data - Pointer to the data itself
// --------------------------------------------------------
VkResult VulkanHelper::CreateStaticBuffer(
	uint64_t dataStride, 
	uint64_t dataCount,
	void* data,
	VkBufferUsageFlags bufferUsage, 
	VkBuffer& buffer,
	VkDeviceMemory& bufferMemory)
{
	// --- STAGING BUFFER for initial CPU -> GPU copy ---
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	{
		// First set up the buffer itself
		VkBufferCreateInfo bufferDesc = {};
		bufferDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferDesc.size = dataStride * dataCount;
		bufferDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_TRY(vkCreateBuffer(vkDevice, &bufferDesc, 0, &stagingBuffer));

		// Next, get the buffer's memory requirements
		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(vkDevice, stagingBuffer, &memReqs);

		// Allocate the actual physical memory
		VkMemoryAllocateInfo memDesc = {};
		memDesc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memDesc.allocationSize = memReqs.size;
		memDesc.memoryTypeIndex = GetMemoryType(
			memReqs,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_TRY(vkAllocateMemory(vkDevice, &memDesc, 0, &stagingMemory));

		// Bind this memory to the buffer
		vkBindBufferMemory(vkDevice, stagingBuffer, stagingMemory, 0);

		// Map, memcpy, unmap into staging buffer
		void* mapped;
		size_t size = dataStride * dataCount;
		vkMapMemory(vkDevice, stagingMemory, 0, size, 0, &mapped);
		memcpy(mapped, data, size);
		vkUnmapMemory(vkDevice, stagingMemory);
	}

	// --- FINAL BUFFER for actual GPU storage ---
	{
		// First set up the buffer itself
		VkBufferCreateInfo bufferDesc = {};
		bufferDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferDesc.size = dataStride * dataCount;
		bufferDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage;
		bufferDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_TRY(vkCreateBuffer(vkDevice, &bufferDesc, 0, &buffer));

		// Next, get the buffer's memory requirements
		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(vkDevice, buffer, &memReqs);

		// Allocate the actual physical memory
		VkMemoryAllocateInfo memDesc = {};
		memDesc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memDesc.allocationSize = memReqs.size;
		memDesc.memoryTypeIndex = GetMemoryType(
			memReqs,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_TRY(vkAllocateMemory(vkDevice, &memDesc, 0, &bufferMemory));

		// Bind this memory to the buffer
		vkBindBufferMemory(vkDevice, buffer, bufferMemory, 0);
	}

	// --- COPY from staging to final ---
	{
		// Begin details
		VkCommandBufferBeginInfo beginDesc = {};
		beginDesc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginDesc.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(vkCommandBuffer, &beginDesc);

		// Copy command
		VkBufferCopy copy = {};
		copy.size = dataStride * dataCount;
		vkCmdCopyBuffer(vkCommandBuffer, stagingBuffer, buffer, 1, &copy);

		// End and submit
		vkEndCommandBuffer(vkCommandBuffer);
		
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &vkCommandBuffer;
		VK_TRY(vkQueueSubmit(vkGraphicsQueue, 1, &submit, 0));
		
		// Wait until finished to proceed
		vkQueueWaitIdle(vkGraphicsQueue);
	}

	return VK_SUCCESS;
}

// --------------------------------------------------------
// Closes the current command list and tells the GPU to
// start executing those commands.  We also wait for
// the GPU to finish this work so we can reset the
// command allocator (which CANNOT be reset while the
// GPU is using its commands) and the command list itself.
// --------------------------------------------------------
void VulkanHelper::CloseExecuteAndResetCommandList()
{
	//// Close the current list and execute it as our only list
	//commandList->Close();
	//ID3D12CommandList* lists[] = { commandList.Get() };
	//commandQueue->ExecuteCommandLists(1, lists);

	//// Always wait before reseting command allocator, as it should not
	//// be reset while the GPU is processing a command list
	//// See: https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12commandallocator-reset
	//WaitForGPU();
	//commandAllocator->Reset();
	//commandList->Reset(commandAllocator.Get(), 0);
}


// --------------------------------------------------------
// Makes our C++ code wait for the GPU to finish its
// current batch of work before moving on.
// --------------------------------------------------------
void VulkanHelper::WaitForGPU()
{
	//// Update our ongoing fence value (a unique index for each "stop sign")
	//// and then place that value into the GPU's command queue
	//waitFenceCounter++;
	//commandQueue->Signal(waitFence.Get(), waitFenceCounter);

	//// Check to see if the most recently completed fence value
	//// is less than the one we just set.
	//if (waitFence->GetCompletedValue() < waitFenceCounter)
	//{
	//	// Tell the fence to let us know when it's hit, and then
	//	// sit and wait until that fence is hit.
	//	waitFence->SetEventOnCompletion(waitFenceCounter, waitFenceEvent);
	//	WaitForSingleObject(waitFenceEvent, INFINITE);
	//}
}