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
	// First set up the buffer itself
	VkBufferCreateInfo bufferDesc = {};
	bufferDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferDesc.size = dataStride * dataCount;
	bufferDesc.usage = bufferUsage;
	bufferDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VkResult bufferResult = vkCreateBuffer(vkDevice, &bufferDesc, 0, &buffer);
	if (bufferResult != VK_SUCCESS)
		return bufferResult;

	// Next, get the buffer's memory requirements and the device's memory properties
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(vkDevice, buffer, &memReqs);

	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memProps);

	// Find the index of the available memory suitable for this buffer
	VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	unsigned int memTypeIndex = -1;
	for (unsigned int i = 0; i < memProps.memoryTypeCount; i++)
	{
		// Validate memory type from requirements
		if (!(memReqs.memoryTypeBits & (1 << i))) continue;

		// Validate memory flags to get proper type of memory
		if (!((memProps.memoryTypes[i].propertyFlags & memFlags) == memFlags)) continue;

		// Success
		memTypeIndex = i;
		break;
	}

	// Did we find suitable memory?
	if (memTypeIndex == -1)
		return VK_ERROR_UNKNOWN;

	// Allocate the actual physical memory
	VkMemoryAllocateInfo memDesc = {};
	memDesc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memDesc.allocationSize = memReqs.size;
	memDesc.memoryTypeIndex = memTypeIndex;

	VkResult allocResult = vkAllocateMemory(vkDevice, &memDesc, 0, &bufferMemory);
	if (allocResult != VK_SUCCESS)
		return allocResult;

	// Bind this memory to the buffer
	vkBindBufferMemory(vkDevice, buffer, bufferMemory, 0);

	// Map, memcpy, unmap
	void* mapped;
	size_t size = dataStride * dataCount;
	vkMapMemory(vkDevice, bufferMemory, 0, size, 0, &mapped);
	memcpy(mapped, data, size);
	vkUnmapMemory(vkDevice, bufferMemory);

	return VK_SUCCESS;
	
	
	//// The overall buffer we'll be creating
	//Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

	//// Describes the final heap
	//D3D12_HEAP_PROPERTIES props = {};
	//props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//props.CreationNodeMask = 1;
	//props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	//props.Type = D3D12_HEAP_TYPE_DEFAULT;
	//props.VisibleNodeMask = 1;

	//D3D12_RESOURCE_DESC desc = {};
	//desc.Alignment = 0;
	//desc.DepthOrArraySize = 1;
	//desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//desc.Format = DXGI_FORMAT_UNKNOWN;
	//desc.Height = 1; // Assuming this is a regular buffer, not a texture
	//desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//desc.MipLevels = 1;
	//desc.SampleDesc.Count = 1;
	//desc.SampleDesc.Quality = 0;
	//desc.Width = (UINT64)dataStride * (UINT64)dataCount; // Size of the buffer

	//device->CreateCommittedResource(
	//	&props,
	//	D3D12_HEAP_FLAG_NONE,
	//	&desc,
	//	D3D12_RESOURCE_STATE_COPY_DEST, // Will eventually be "common", but we're copying to it first!
	//	0,
	//	IID_PPV_ARGS(buffer.GetAddressOf()));

	//// Now create an intermediate upload heap for copying initial data
	//D3D12_HEAP_PROPERTIES uploadProps = {};
	//uploadProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//uploadProps.CreationNodeMask = 1;
	//uploadProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	//uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Can only ever be Generic_Read state
	//uploadProps.VisibleNodeMask = 1;

	//Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap;
	//device->CreateCommittedResource(
	//	&uploadProps,
	//	D3D12_HEAP_FLAG_NONE,
	//	&desc,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	0,
	//	IID_PPV_ARGS(uploadHeap.GetAddressOf()));

	//// Do a straight map/memcpy/unmap
	//void* gpuAddress = 0;
	//uploadHeap->Map(0, 0, &gpuAddress);
	//memcpy(gpuAddress, data, dataStride * dataCount);
	//uploadHeap->Unmap(0, 0);

	//// Copy the whole buffer from uploadheap to vert buffer
	//commandList->CopyResource(buffer.Get(), uploadHeap.Get());

	//// Transition the buffer to generic read for the rest of the app lifetime (presumable)
	//D3D12_RESOURCE_BARRIER rb = {};
	//rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//rb.Transition.pResource = buffer.Get();
	//rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	//rb.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	//rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	//commandList->ResourceBarrier(1, &rb);

	//// Execute the command list and return the finished buffer
	//CloseExecuteAndResetCommandList();
	//return buffer;
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