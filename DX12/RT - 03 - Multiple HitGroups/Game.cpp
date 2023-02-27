#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "BufferStructs.h"
#include "DX12Helper.h"
#include "Material.h"
#include "Helpers.h"
#include "RaytracingHelper.h"

#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		// The application's handle
		L"DirectX Game",// Text for the window's title bar
		1280,			// Width of the window's client area
		720,			// Height of the window's client area
		false,			// Sync the framerate to the monitor refresh? (lock framerate)
		true),			// Show extra stats (fps) in title bar?
	lightCount(32)
{

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in Game

	// However, we DO need to wait here until the GPU
	// is actually done with its work
	DX12Helper::GetInstance().WaitForGPU();

	// Clean up non-smart pointer objects
	delete& RaytracingHelper::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Attempt to initialize DXR
	RaytracingHelper::GetInstance().Initialize(
		windowWidth,
		windowHeight,
		device, 
		commandQueue, 
		commandList, 
		FixPath(L"Raytracing.cso"));

	// Seed random
	srand((unsigned int)time(0));

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	CreateRootSigAndPipelineState();
	CreateBasicGeometry();
	GenerateLights();

	camera = std::make_shared<Camera>(
		XMFLOAT3(0.0f, 0.0f, -8.0f),	// Position
		5.0f,							// Move speed
		0.002f,							// Look speed
		XM_PIDIV4,						// Field of view
		windowWidth / (float)windowHeight);	// Aspect ratio

	// Ensure the command list is closed going into Draw for the first time
	commandList->Close();
}


// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature 
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;

	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"PixelShader.cso").c_str(), pixelShaderByteCode.GetAddressOf());
	}

	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	{
		// Create an input layout that describes the vertex format
		// used by the vertex shader we're using
		//  - This is used by the pipeline to know how to interpret the raw data
		//     sitting inside a vertex buffer

		// Set up the first element - a position, which is 3 float values
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // How far into the vertex is this?  Assume it's after the previous element
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// Most formats are described as color channels, really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";					// This is "POSITION" - needs to match the semantics in our vertex shader input!
		inputElements[0].SemanticIndex = 0;							// This is the 0th position (there could be more)

		// Set up the second element - a UV, which is 2 more float values
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;			// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";					// Match our vertex shader input!
		inputElements[1].SemanticIndex = 0;							// This is the 0th uv (there could be more)

		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";					// Match our vertex shader input!
		inputElements[2].SemanticIndex = 0;							// This is the 0th normal (there could be more)

		// Set up the fourth element - a tangent, which is 2 more float values
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";					// Match our vertex shader input!
		inputElements[3].SemanticIndex = 0;							// This is the 0th tangent (there could be more)
	}

	// Root Signature
	{
		// Describe the range of CBVs needed for the vertex shader
		D3D12_DESCRIPTOR_RANGE cbvRangeVS = {};
		cbvRangeVS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeVS.NumDescriptors = 1;
		cbvRangeVS.BaseShaderRegister = 0;
		cbvRangeVS.RegisterSpace = 0;
		cbvRangeVS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the range of CBVs needed for the pixel shader
		D3D12_DESCRIPTOR_RANGE cbvRangePS = {};
		cbvRangePS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangePS.NumDescriptors = 1;
		cbvRangePS.BaseShaderRegister = 0;
		cbvRangePS.RegisterSpace = 0;
		cbvRangePS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create a range of SRV's for textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4;		// Set to max number of textures at once (match pixel shader!)
		srvRange.BaseShaderRegister = 0;	// Starts at s0 (match pixel shader!)
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[3] = {};

		// CBV table param for vertex shader
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVS;

		// CBV table param for vertex shader
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangePS;

		// SRV table param
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;

		// Create a single static sampler (available to all pixel shaders at the same slot)
		// Note: This is in lieu of having materials have their own samplers for this demo
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0;  // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		
		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSig.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != 0)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignature.GetAddressOf()));
	}

	// Pipeline state
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		// Overall primitive topology type (triangle, line, etc.) is set here 
		// IASetPrimTop() is still used to set list/strip/adj options
		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();

		// -- Render targets ---
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
	}
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Quick macro to simplify texture loading lines below
#define LoadTexture(x) DX12Helper::GetInstance().LoadTexture(FixPath(x).c_str())

	// Load textures
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneAlbedo = LoadTexture(L"../../../../Assets/Textures/cobblestone_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneNormals = LoadTexture(L"../../../../Assets/Textures/cobblestone_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneRoughness = LoadTexture(L"../../../../Assets/Textures/cobblestone_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneMetal = LoadTexture(L"../../../../Assets/Textures/cobblestone_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE bronzeAlbedo = LoadTexture(L"../../../../Assets/Textures/bronze_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeNormals = LoadTexture(L"../../../../Assets/Textures/bronze_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeRoughness = LoadTexture(L"../../../../Assets/Textures/bronze_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeMetal = LoadTexture(L"../../../../Assets/Textures/bronze_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE scratchedAlbedo = LoadTexture(L"../../../../Assets/Textures/scratched_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedNormals = LoadTexture(L"../../../../Assets/Textures/scratched_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedRoughness = LoadTexture(L"../../../../Assets/Textures/scratched_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedMetal = LoadTexture(L"../../../../Assets/Textures/scratched_metal.png");

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> greyDiffuse = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.5f, 0.5f), 1.0f);
	std::shared_ptr<Material> metal = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.6f, 0.7f), 0.0f);

	// Load meshes
	std::shared_ptr<Mesh> cube		= std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/cube.obj").c_str());
	std::shared_ptr<Mesh> sphere	= std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/sphere.obj").c_str());
	std::shared_ptr<Mesh> helix		= std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/helix.obj").c_str());
	std::shared_ptr<Mesh> torus		= std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/torus.obj").c_str());
	std::shared_ptr<Mesh> cylinder	= std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/cylinder.obj").c_str());

	// Floor
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cube, greyDiffuse);
	floor->GetTransform()->SetScale(100);
	floor->GetTransform()->SetPosition(0, -52, 0);
	entities.push_back(floor);

	// Spinning torus
	std::shared_ptr<GameEntity> t = std::make_shared<GameEntity>(torus, metal);
	t->GetTransform()->SetScale(2);
	t->GetTransform()->SetPosition(0, 2, 0);
	entities.push_back(t);

	// Four floating transparent spheres
	std::shared_ptr<Material> glassWhite	= std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1), 0.0f, true);
	std::shared_ptr<Material> glassRed		= std::make_shared<Material>(pipelineState, XMFLOAT3(1, 0, 0), 0.0f, true);
	std::shared_ptr<Material> glassGreen	= std::make_shared<Material>(pipelineState, XMFLOAT3(0, 1, 0), 0.0f, true);
	std::shared_ptr<Material> glassBlue		= std::make_shared<Material>(pipelineState, XMFLOAT3(0, 0, 1), 0.0f, true);

	std::shared_ptr<GameEntity> glassSphereWhite	= std::make_shared<GameEntity>(sphere, glassWhite);
	std::shared_ptr<GameEntity> glassSphereRed		= std::make_shared<GameEntity>(sphere, glassRed);
	std::shared_ptr<GameEntity> glassSphereGreen	= std::make_shared<GameEntity>(sphere, glassGreen);
	std::shared_ptr<GameEntity> glassSphereBlue		= std::make_shared<GameEntity>(sphere, glassBlue);
	
	glassSphereWhite->GetTransform()->SetPosition(0, 1, -2);
	glassSphereRed  ->GetTransform()->SetPosition(2, 1, 0);
	glassSphereGreen->GetTransform()->SetPosition(0, 1, 2);
	glassSphereBlue ->GetTransform()->SetPosition(-2, 1, 0);

	entities.push_back(glassSphereWhite);
	entities.push_back(glassSphereRed);
	entities.push_back(glassSphereGreen);
	entities.push_back(glassSphereBlue);

	std::shared_ptr<GameEntity> parent = std::make_shared<GameEntity>(cube, greyDiffuse);
	parent->GetTransform()->SetPosition(0, -50, 0); // Move so we can't see it
	parent->GetTransform()->AddChild(glassSphereWhite->GetTransform());
	parent->GetTransform()->AddChild(glassSphereRed->GetTransform());
	parent->GetTransform()->AddChild(glassSphereGreen->GetTransform());
	parent->GetTransform()->AddChild(glassSphereBlue->GetTransform());
	entities.push_back(parent);

	float range = 10;
	for (int i = 0; i < 50; i++)
	{
		// Random roughness
		float rough = RandomRange(0.0f, 1.0f) > 0.5f ? 0.0f : 1.0f;

		std::shared_ptr<Material> mat = std::make_shared<Material>(
			pipelineState, 
			XMFLOAT3(
				RandomRange(0.0f, 1.0f),
				RandomRange(0.0f, 1.0f),
				RandomRange(0.0f, 1.0f)
			),
			rough);

		float scale = RandomRange(0.5f, 1.5f);

		std::shared_ptr<GameEntity> sphereEnt = std::make_shared<GameEntity>(sphere, mat);
		sphereEnt->GetTransform()->SetScale(scale);
		sphereEnt->GetTransform()->SetPosition(
			RandomRange(-range, range),
			-2 + scale / 2.0f,
			RandomRange(-range, range));

		entities.push_back(sphereEnt);
	}

	// Since meshes create their own BLAS's, we just need to create the TLAS for the scene
	RaytracingHelper::GetInstance().CreateTopLevelAccelerationStructureForScene(entities);
}


void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-2.0f, 5.0f), RandomRange(-15.0f, 15.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}
	
	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update the camera's projection to match the new size
	if (camera)
	{
		camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
	}

	RaytracingHelper::GetInstance().ResizeOutputUAV(windowWidth, windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	entities[1]->GetTransform()->Rotate(deltaTime * 0.5f, deltaTime * 0.5f, deltaTime * 0.5f);
	entities[6]->GetTransform()->Rotate(0, deltaTime * 0.1f, 0);

	// Skip the first few entities:
	// 0: floor
	// 1: torus
	// 2,3,4,5: transparent balls
	// 6: ball parent
	int skip = 7;

	// Rotate entities (skip first two)
	float range = 20;
	for(int i = skip; i < entities.size(); i++)
	{
		//e->GetTransform()->Rotate(0, deltaTime * 0.5f, 0);
		
		XMFLOAT3 pos = entities[i]->GetTransform()->GetPosition();
		switch (i % 2)
		{
		case 0:
			pos.x = sin((totalTime + i) * (4/range)) * range;
			break;

		case 1:
			pos.z = sin((totalTime + i) * (4/range)) * range;
			break;
		}
		entities[i]->GetTransform()->SetPosition(pos);
	}


	// Update other objects
	camera->Update(deltaTime);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Grab the helper
	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	// Reset allocator associated with the current buffer
	// and set up the command list to use that allocator
	commandAllocators[currentSwapBuffer]->Reset();
	commandList->Reset(commandAllocators[currentSwapBuffer].Get(), 0);

	// Grab the current back buffer for this frame
	Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = backBuffers[currentSwapBuffer];

	// Raytracing here!
	{
		// Update raytracing accel structure
		RaytracingHelper::GetInstance().CreateTopLevelAccelerationStructureForScene(entities);

		// Perform raytrace
		RaytracingHelper::GetInstance().Raytrace(camera, backBuffers[currentSwapBuffer], currentSwapBuffer);
	}

	// Finish the frame
	{
		// Present the current back buffer
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Wait to proceed to the next frame until the associated buffer is ready
		currentSwapBuffer = dx12Helper.SyncSwapChain(currentSwapBuffer);
	}

}