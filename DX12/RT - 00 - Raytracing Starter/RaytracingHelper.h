#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>

#include "Mesh.h"
#include "Camera.h"

class RaytracingHelper
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static RaytracingHelper& GetInstance()
	{
		if (!instance)
		{
			instance = new RaytracingHelper();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	RaytracingHelper(RaytracingHelper const&) = delete;
	void operator=(RaytracingHelper const&) = delete;

private:
	static RaytracingHelper* instance;
	RaytracingHelper() :
		dxrAvailable(false),
		helperInitialized(false),
		raytracingOutputUAV_CPU{},
		raytracingOutputUAV_GPU{},
		screenHeight(1),
		screenWidth(1),
		shaderTableRecordSize(0)
	{};
#pragma endregion

public:
	~RaytracingHelper();

	// Initialization for singleton
	void Initialize(
		unsigned int screenWidth,
		unsigned int screenHeight,
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		std::wstring raytracingShaderLibraryFile
	);

	// Resizing when window resizes
	void ResizeOutputUAV(unsigned int screenWidth, unsigned int screenHeight);

	// Setup process requiring data from outside the helper
	void CreateBottomLevelAccelerationStructure(std::shared_ptr<Mesh> mesh);
	void CreateTopLevelAccelerationStructure();

	// Actual work
	void Raytrace(std::shared_ptr<Camera> camera, Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer);

private:

	unsigned int screenWidth;
	unsigned int screenHeight;

	// Is raytracing (DirectX Raytracing - DXR) available on this hardware?
	bool dxrAvailable;
	bool helperInitialized;

	// Command queue for processing raytracing commands
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	// Raytracing-specific versions of some base DX12 objects
	Microsoft::WRL::ComPtr<ID3D12Device5> dxrDevice;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;

	// Root signatures for basic raytracing
	Microsoft::WRL::ComPtr<ID3D12RootSignature> globalRaytracingRootSig;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> localRaytracingRootSig;

	// Overall raytracing pipeline state object
	// This is similar to a regular PSO, but without the standard
	// rasterization pipeline stuff.  Also grabbing the properties
	// so we can get shader IDs out of it later.
	Microsoft::WRL::ComPtr<ID3D12StateObject> raytracingPipelineStateObject;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> raytracingPipelineProperties;

	// Shader table holding shaders for use during raytracing
	Microsoft::WRL::ComPtr<ID3D12Resource> shaderTable;
	UINT64 shaderTableRecordSize;

	// Accel structure requirements
	Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratchBuffer; 
	Microsoft::WRL::ComPtr<ID3D12Resource> blasScratchBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> tlasInstanceDescBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> topLevelAccelerationStructure;
	Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAccelerationStructure;

	// Actual output resource
	Microsoft::WRL::ComPtr<ID3D12Resource> raytracingOutput;
	D3D12_CPU_DESCRIPTOR_HANDLE raytracingOutputUAV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputUAV_GPU;

	// Other SRVs for geometry
	// - Larger application will need these FOR EACH MESH
	D3D12_GPU_DESCRIPTOR_HANDLE indexBufferSRV;
	D3D12_GPU_DESCRIPTOR_HANDLE vertexBufferSRV;

	// Helper functions for each initalization step
	void CreateRaytracingRootSignatures();
	void CreateRaytracingPipelineState(std::wstring raytracingShaderLibraryFile);
	void CreateShaderTable();
	void CreateRaytracingOutputUAV(unsigned int width, unsigned int height);
};

