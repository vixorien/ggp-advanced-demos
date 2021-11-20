#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

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
	DX12Helper() {};
#pragma endregion

public:
	~DX12Helper();

	// Initialization for singleton
	void Initialize(
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator
	);

	// Resource creation
	HRESULT CreateStaticBuffer(unsigned int dataStride, unsigned int dataCount, void* data, ID3D12Resource** buffer);
	
	// Dynamic resources
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetConstantBufferDescriptorHeap();
	D3D12_GPU_DESCRIPTOR_HANDLE FillNextConstantBufferAndGetGPUDescriptorHandle(
		void* data,
		unsigned int dataSizeInBytes);


	// Command list & synchronization
	void CloseExecuteAndResetCommandList();
	void WaitForGPU();


private:

	// Overall device
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	// Command list related
	// Note: We're assuming a single command list for the entire
	// engine at this point.  That's not always true for more
	// complex engines but should be fine for us
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		commandAllocator;

	// Basic CPU/GPU synchronization
	Microsoft::WRL::ComPtr<ID3D12Fence> waitFence;
	HANDLE								waitFenceEvent;
	unsigned long						waitFenceCounter;

	// Maximum number of constant buffers, assuming each buffer
	// is 256 bytes or less.  Larger buffers are fine, but will
	// result in fewer buffers in use at any time
	const int maxConstantBuffers = 1000;
	
	// GPU-side contant buffer upload heap
	Microsoft::WRL::ComPtr<ID3D12Resource> cbUploadHeap;
	UINT64 cbUploadHeapSizeInBytes;
	UINT64 cbUploadHeapOffsetInBytes;
	void* cbUploadHeapStartAddress;

	// GPU-side CBV/SRV descriptor heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvDescriptorHeap;
	SIZE_T cbvDescriptorHeapIncrementSize;
	unsigned int cbvDescriptorOffset;

	void CreateConstantBufferUploadHeap();
	void CreateConstantBufferViewDescriptorHeap();
};

