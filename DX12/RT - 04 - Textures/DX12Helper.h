#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

class DX12Helper
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static DX12Helper& GetInstance()
	{
		if (!instance)
		{
			instance = new DX12Helper();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	DX12Helper(DX12Helper const&) = delete;
	void operator=(DX12Helper const&) = delete;

private:
	static DX12Helper* instance;
	DX12Helper() :
		numBackBuffers(1),
		cbUploadHeapOffsetInBytes(0),
		cbUploadHeapSizeInBytes(0),
		cbUploadHeapStartAddress(0),
		cbvDescriptorOffset(0),
		cbvSrvDescriptorHeapIncrementSize(0),
		srvDescriptorOffset(0),
		waitFence(0),
		waitFenceCounter(0),
		waitFenceEvent(0),
		frameSyncFence(0),
		frameSyncFenceCounters{},
		frameSyncFenceEvent(0),
		commandAllocators{}
	{};
#pragma endregion

public:
	~DX12Helper();

	// Initialization for singleton
	void Initialize(
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>* commandAllocators,
		unsigned int numBackBuffers
	);

	// Getters
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetCBVSRVDescriptorHeap();
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetDefaultAllocator();
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocatorByIndex(unsigned int index);

	// Resource creation
	D3D12_CPU_DESCRIPTOR_HANDLE LoadTexture(const wchar_t* file, bool generateMips = true);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		UINT64 size,
		D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
		UINT64 alignment = 0);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateStaticBuffer(unsigned int dataStride, unsigned int dataCount, void* data);
	
	// Resource usage
	D3D12_GPU_DESCRIPTOR_HANDLE FillNextConstantBufferAndGetGPUDescriptorHandle(
		void* data,
		unsigned int dataSizeInBytes);
	D3D12_GPU_DESCRIPTOR_HANDLE CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE firstDescriptorToCopy, unsigned int numDescriptorsToCopy);

	void ReserveSrvUavDescriptorHeapSlot(D3D12_CPU_DESCRIPTOR_HANDLE* reservedCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE* reservedGPUHandle);

	UINT GetDescriptorIndex(D3D12_GPU_DESCRIPTOR_HANDLE handle);

	// Command list & basic synchronization
	void ExecuteCommandList();
	void WaitForGPU();
	unsigned int SyncSwapChain(unsigned int currentSwapBufferIndex);

private:

	// Back buffer count for frame syncing
	unsigned int numBackBuffers;

	// Overall device
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	// Command list related
	// Note: We're assuming a single command list for the entire
	// engine at this point.  That's not always true for more
	// complex engines but should be fine for us
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>*		commandAllocators;

	// Basic CPU/GPU synchronization
	Microsoft::WRL::ComPtr<ID3D12Fence> waitFence;
	HANDLE								waitFenceEvent;
	UINT64								waitFenceCounter;

	// Frame sync'ing
	Microsoft::WRL::ComPtr<ID3D12Fence> frameSyncFence;
	HANDLE								frameSyncFenceEvent;
	UINT64*								frameSyncFenceCounters;

	// Maximum number of constant buffers, assuming each buffer
	// is 256 bytes or less.  Larger buffers are fine, but will
	// result in fewer buffers in use at any time.
	// This is also used as the max number of CBVs possible.
	const unsigned int maxConstantBuffers = 1000;

	// Maximum number of texture descriptors (SRVs) we can have.
	// Each material will have a chunk of this, plus any 
	// non-material textures we may need for our program.
	// Note: If we delayed the creation of this heap until 
	//       after all textures and materials were created,
	//       we could come up with an exact amount.  The following
	//       constant ensures we (hopefully) never run out of room.
	const unsigned int maxTextureDescriptors = 1000;
	
	// GPU-side contant buffer upload heap
	Microsoft::WRL::ComPtr<ID3D12Resource> cbUploadHeap;
	UINT64 cbUploadHeapSizeInBytes;
	UINT64 cbUploadHeapOffsetInBytes;
	void* cbUploadHeapStartAddress;

	// GPU-side CBV/SRV descriptor heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvDescriptorHeap;
	SIZE_T cbvSrvDescriptorHeapIncrementSize;
	unsigned int cbvDescriptorOffset;
	unsigned int srvDescriptorOffset;

	void CreateConstantBufferUploadHeap();
	void CreateCBVSRVDescriptorHeap();

	// Textures
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> cpuSideTextureDescriptorHeaps;
};

