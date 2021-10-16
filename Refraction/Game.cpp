
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"

#include "WICTextureLoader.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

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
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	camera = 0;

	// Seed random
	srand((unsigned int)time(0));

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
	//   to call Release() on each DirectX object

	// Clean up our other resources
	for (auto& m : materials) delete m;
	for (auto& e : entities) delete e;

	// Delete any one-off objects
	delete sky;
	delete camera;
	delete renderer;

	// Delete singletons
	delete& Input::GetInstance();
	delete& Assets::GetInstance();

	// IMGUI
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// IMGUI
	{
		// Initialize ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		// Pick a style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX11_Init(device.Get(), context.Get());
	}

	// Initialize the input manager with the window's handle
	Input::GetInstance().Initialize(this->hWnd);

	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Make our camera
	camera = new Camera(
		0, 0, -10,	// Position
		3.0f,		// Move speed
		1.0f,		// Mouse look
		this->width / (float)this->height); // Aspect ratio

	// Create the renderer (last since we need some other pieces like the Sky)
	renderer = new Renderer(
		entities,
		lights,
		MAX_LIGHTS / 2, // Half the maximum lights are active to start with
		sky,
		width,
		height,
		device,
		context,
		swapChain,
		backBufferRTV,
		depthStencilView);


	// Set up lights once the renderer is active,
	// as that now tracks the active light count
	GenerateLights();
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	Assets& assets = Assets::GetInstance();
	assets.Initialize("..\\..\\..\\Assets\\", device, context);
	assets.LoadAllAssets();

	// Create a random texture for SSAO
	const int textureSize = 4;
	const int totalPixels = textureSize * textureSize;
	XMFLOAT4 randomPixels[totalPixels] = {};
	for (int i = 0; i < totalPixels; i++)
	{
		XMVECTOR randomVec = XMVectorSet(RandomRange(-1, 1), RandomRange(-1, 1), 0, 0);
		XMStoreFloat4(&randomPixels[i], XMVector3Normalize(randomVec));
	}
	assets.CreateFloatTexture("random", textureSize, textureSize, randomPixels);


	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());

	// Also create a clamp sampler
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());


	// Create the sky
	sky = new Sky(
		/*assets.GetTexture("Skies\\SunnyCubeMap.dds"),*/
		assets.GetTexture("Skies\\Clouds Blue\\right.png"),
		assets.GetTexture("Skies\\Clouds Blue\\left.png"),
		assets.GetTexture("Skies\\Clouds Blue\\up.png"),
		assets.GetTexture("Skies\\Clouds Blue\\down.png"),
		assets.GetTexture("Skies\\Clouds Blue\\front.png"),
		assets.GetTexture("Skies\\Clouds Blue\\back.png"),
		samplerOptions,
		device,
		context);
	
	// Grab basic shaders for all these materials
	SimpleVertexShader* vs = assets.GetVertexShader("VertexShader.cso");
	SimplePixelShader* ps = assets.GetPixelShader("PixelShader.cso");
	SimplePixelShader* psPBR = assets.GetPixelShader("PixelShaderPBR.cso");

	// Create basic materials
	Material* cobbleMat2x = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	cobbleMat2x->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\cobblestone_albedo.png"));
	cobbleMat2x->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\cobblestone_normals.png"));
	cobbleMat2x->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\cobblestone_roughness.png"));
	cobbleMat2x->AddPSSampler("BasicSampler", samplerOptions);

	Material* floorMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	floorMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\floor_albedo.png"));
	floorMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\floor_normals.png"));
	floorMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\floor_roughness.png"));
	floorMat->AddPSSampler("BasicSampler", samplerOptions);

	Material* paintMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	paintMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\paint_albedo.png"));
	paintMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\paint_normals.png"));
	paintMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\paint_roughness.png"));
	paintMat->AddPSSampler("BasicSampler", samplerOptions);

	Material* scratchedMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	scratchedMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\scratched_albedo.png"));
	scratchedMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\scratched_normals.png"));
	scratchedMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\scratched_roughness.png"));
	scratchedMat->AddPSSampler("BasicSampler", samplerOptions);

	Material* bronzeMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	bronzeMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\bronze_albedo.png"));
	bronzeMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\bronze_normals.png"));
	bronzeMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\bronze_roughness.png"));
	bronzeMat->AddPSSampler("BasicSampler", samplerOptions);

	Material* roughMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	roughMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\rough_albedo.png"));
	roughMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\rough_normals.png"));
	roughMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\rough_roughness.png"));
	roughMat->AddPSSampler("BasicSampler", samplerOptions);

	Material* woodMat = new Material(vs, ps, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	woodMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\wood_albedo.png"));
	woodMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\wood_normals.png"));
	woodMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\wood_roughness.png"));
	woodMat->AddPSSampler("BasicSampler", samplerOptions);


	materials.push_back(cobbleMat2x);
	materials.push_back(floorMat);
	materials.push_back(paintMat);
	materials.push_back(scratchedMat);
	materials.push_back(bronzeMat);
	materials.push_back(roughMat);
	materials.push_back(woodMat);

	// Create PBR materials
	Material* cobbleMat2xPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	cobbleMat2xPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\cobblestone_albedo.png"));
	cobbleMat2xPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\cobblestone_normals.png"));
	cobbleMat2xPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\cobblestone_roughness.png"));
	cobbleMat2xPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\cobblestone_metal.png"));
	cobbleMat2xPBR->AddPSSampler("BasicSampler", samplerOptions);
	cobbleMat2xPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* floorMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	floorMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\floor_albedo.png"));
	floorMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\floor_normals.png"));
	floorMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\floor_roughness.png"));
	floorMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\floor_metal.png"));
	floorMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	floorMatPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* paintMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	paintMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\paint_albedo.png"));
	paintMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\paint_normals.png"));
	paintMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\paint_roughness.png"));
	paintMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\paint_metal.png"));
	paintMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	paintMatPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* scratchedMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	scratchedMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\scratched_albedo.png"));
	scratchedMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\scratched_normals.png"));
	scratchedMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\scratched_roughness.png"));
	scratchedMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\scratched_metal.png"));
	scratchedMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	scratchedMatPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* bronzeMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	bronzeMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\bronze_albedo.png"));
	bronzeMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\bronze_normals.png"));
	bronzeMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\bronze_roughness.png"));
	bronzeMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\bronze_metal.png"));
	bronzeMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	bronzeMatPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* roughMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	roughMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\rough_albedo.png"));
	roughMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\rough_normals.png"));
	roughMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\rough_roughness.png"));
	roughMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\rough_metal.png"));
	roughMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	roughMatPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* woodMatPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	woodMatPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\wood_albedo.png"));
	woodMatPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\wood_normals.png"));
	woodMatPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\wood_roughness.png"));
	woodMatPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\wood_metal.png"));
	woodMatPBR->AddPSSampler("BasicSampler", samplerOptions);
	woodMatPBR->AddPSSampler("ClampSampler", clampSampler);

	materials.push_back(cobbleMat2xPBR);
	materials.push_back(floorMatPBR);
	materials.push_back(paintMatPBR);
	materials.push_back(scratchedMatPBR);
	materials.push_back(bronzeMatPBR);
	materials.push_back(roughMatPBR);
	materials.push_back(woodMatPBR);

	// === Create the PBR entities =====================================
	Mesh* sphereMesh = assets.GetMesh("Models\\sphere.obj");

	GameEntity* cobSpherePBR = new GameEntity(sphereMesh, cobbleMat2xPBR);
	cobSpherePBR->GetTransform()->SetScale(2, 2, 2);
	cobSpherePBR->GetTransform()->SetPosition(-6, 2, 0);

	GameEntity* floorSpherePBR = new GameEntity(sphereMesh, floorMatPBR);
	floorSpherePBR->GetTransform()->SetScale(2, 2, 2);
	floorSpherePBR->GetTransform()->SetPosition(-4, 2, 0);

	GameEntity* paintSpherePBR = new GameEntity(sphereMesh, paintMatPBR);
	paintSpherePBR->GetTransform()->SetScale(2, 2, 2);
	paintSpherePBR->GetTransform()->SetPosition(-2, 2, 0);

	GameEntity* scratchSpherePBR = new GameEntity(sphereMesh, scratchedMatPBR);
	scratchSpherePBR->GetTransform()->SetScale(2, 2, 2);
	scratchSpherePBR->GetTransform()->SetPosition(0, 2, 0);

	GameEntity* bronzeSpherePBR = new GameEntity(sphereMesh, bronzeMatPBR);
	bronzeSpherePBR->GetTransform()->SetScale(2, 2, 2);
	bronzeSpherePBR->GetTransform()->SetPosition(2, 2, 0);

	GameEntity* roughSpherePBR = new GameEntity(sphereMesh, roughMatPBR);
	roughSpherePBR->GetTransform()->SetScale(2, 2, 2);
	roughSpherePBR->GetTransform()->SetPosition(4, 2, 0);

	GameEntity* woodSpherePBR = new GameEntity(sphereMesh, woodMatPBR);
	woodSpherePBR->GetTransform()->SetScale(2, 2, 2);
	woodSpherePBR->GetTransform()->SetPosition(6, 2, 0);

	entities.push_back(cobSpherePBR);
	entities.push_back(floorSpherePBR);
	entities.push_back(paintSpherePBR);
	entities.push_back(scratchSpherePBR);
	entities.push_back(bronzeSpherePBR);
	entities.push_back(roughSpherePBR);
	entities.push_back(woodSpherePBR);

	// Create the non-PBR entities ==============================
	GameEntity* cobSphere = new GameEntity(sphereMesh, cobbleMat2x);
	cobSphere->GetTransform()->SetScale(2, 2, 2);
	cobSphere->GetTransform()->SetPosition(-6, -2, 0);

	GameEntity* floorSphere = new GameEntity(sphereMesh, floorMat);
	floorSphere->GetTransform()->SetScale(2, 2, 2);
	floorSphere->GetTransform()->SetPosition(-4, -2, 0);

	GameEntity* paintSphere = new GameEntity(sphereMesh, paintMat);
	paintSphere->GetTransform()->SetScale(2, 2, 2);
	paintSphere->GetTransform()->SetPosition(-2, -2, 0);

	GameEntity* scratchSphere = new GameEntity(sphereMesh, scratchedMat);
	scratchSphere->GetTransform()->SetScale(2, 2, 2);
	scratchSphere->GetTransform()->SetPosition(0, -2, 0);

	GameEntity* bronzeSphere = new GameEntity(sphereMesh, bronzeMat);
	bronzeSphere->GetTransform()->SetScale(2, 2, 2);
	bronzeSphere->GetTransform()->SetPosition(2, -2, 0);

	GameEntity* roughSphere = new GameEntity(sphereMesh, roughMat);
	roughSphere->GetTransform()->SetScale(2, 2, 2);
	roughSphere->GetTransform()->SetPosition(4, -2, 0);

	GameEntity* woodSphere = new GameEntity(sphereMesh, woodMat);
	woodSphere->GetTransform()->SetScale(2, 2, 2);
	woodSphere->GetTransform()->SetPosition(6, -2, 0);

	entities.push_back(cobSphere);
	entities.push_back(floorSphere);
	entities.push_back(paintSphere);
	entities.push_back(scratchSphere);
	entities.push_back(bronzeSphere);
	entities.push_back(roughSphere);
	entities.push_back(woodSphere);


	// Create simple PBR materials & entities (mostly for IBL testing)
	assets.CreateSolidColorTexture("white", 2, 2, XMFLOAT4(1, 1, 1, 1));
	assets.CreateSolidColorTexture("black", 2, 2, XMFLOAT4(0, 0, 0, 0));
	assets.CreateSolidColorTexture("grey", 2, 2, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));
	assets.CreateSolidColorTexture("darkGrey", 2, 2, XMFLOAT4(0.25f, 0.25f, 0.25f, 1));
	assets.CreateSolidColorTexture("flatNormalMap", 2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f));

	Material* solidShinyMetal = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidShinyMetal->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidShinyMetal->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidShinyMetal->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("black"));
	solidShinyMetal->AddPSTextureSRV("MetalTexture", assets.GetTexture("white"));
	solidShinyMetal->AddPSSampler("BasicSampler", samplerOptions);
	solidShinyMetal->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidShinyMetal);

	Material* solidQuarterRoughMetal = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidQuarterRoughMetal->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidQuarterRoughMetal->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidQuarterRoughMetal->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("darkGrey"));
	solidQuarterRoughMetal->AddPSTextureSRV("MetalTexture", assets.GetTexture("white"));
	solidQuarterRoughMetal->AddPSSampler("BasicSampler", samplerOptions);
	solidQuarterRoughMetal->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidQuarterRoughMetal);

	Material* solidHalfRoughMetal = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidHalfRoughMetal->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidHalfRoughMetal->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidHalfRoughMetal->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("grey"));
	solidHalfRoughMetal->AddPSTextureSRV("MetalTexture", assets.GetTexture("white"));
	solidHalfRoughMetal->AddPSSampler("BasicSampler", samplerOptions);
	solidHalfRoughMetal->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidHalfRoughMetal);

	Material* solidShinyPlastic = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidShinyPlastic->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidShinyPlastic->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidShinyPlastic->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("black"));
	solidShinyPlastic->AddPSTextureSRV("MetalTexture", assets.GetTexture("black"));
	solidShinyPlastic->AddPSSampler("BasicSampler", samplerOptions);
	solidShinyPlastic->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidShinyPlastic);

	Material* solidQuarterRoughPlastic = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidQuarterRoughPlastic->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidQuarterRoughPlastic->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidQuarterRoughPlastic->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("darkGrey"));
	solidQuarterRoughPlastic->AddPSTextureSRV("MetalTexture", assets.GetTexture("black"));
	solidQuarterRoughPlastic->AddPSSampler("BasicSampler", samplerOptions);
	solidQuarterRoughPlastic->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidQuarterRoughPlastic);

	Material* solidHalfRoughPlastic = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	solidHalfRoughPlastic->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("white"));
	solidHalfRoughPlastic->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	solidHalfRoughPlastic->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("grey"));
	solidHalfRoughPlastic->AddPSTextureSRV("MetalTexture", assets.GetTexture("black"));
	solidHalfRoughPlastic->AddPSSampler("BasicSampler", samplerOptions);
	solidHalfRoughPlastic->AddPSSampler("ClampSampler", clampSampler);
	materials.push_back(solidHalfRoughPlastic);



	GameEntity* shinyMetal = new GameEntity(sphereMesh, solidShinyMetal);
	shinyMetal->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(shinyMetal);

	GameEntity* quarterRoughMetal = new GameEntity(sphereMesh, solidQuarterRoughMetal);
	quarterRoughMetal->GetTransform()->SetPosition(-3.5f, 0, 0);
	entities.push_back(quarterRoughMetal);

	GameEntity* roughMetal = new GameEntity(sphereMesh, solidHalfRoughMetal);
	roughMetal->GetTransform()->SetPosition(-2, 0, 0);
	entities.push_back(roughMetal);

	GameEntity* shinyPlastic = new GameEntity(sphereMesh, solidShinyPlastic);
	shinyPlastic->GetTransform()->SetPosition(2, 0, 0);
	entities.push_back(shinyPlastic);

	GameEntity* quarterRoughPlastic = new GameEntity(sphereMesh, solidQuarterRoughPlastic);
	quarterRoughPlastic->GetTransform()->SetPosition(3.5f, 0, 0);
	entities.push_back(quarterRoughPlastic);

	GameEntity* roughPlastic = new GameEntity(sphereMesh, solidHalfRoughPlastic);
	roughPlastic->GetTransform()->SetPosition(5, 0, 0);
	entities.push_back(roughPlastic);


	// Create a low poly tree
	Material* treeMat = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 0.0f, XMFLOAT2(1, 1));
	treeMat->AddPSSampler("BasicSampler", samplerOptions);
	treeMat->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\lowpoly tree.png"));
	treeMat->AddPSTextureSRV("NormalTexture", assets.GetTexture("flatNormalMap"));
	treeMat->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("white"));
	treeMat->AddPSTextureSRV("MetalTexture", assets.GetTexture("black"));
	materials.push_back(treeMat);
	
	GameEntity* tree = new GameEntity(assets.GetMesh("Models\\lowpoly tree.obj"), treeMat);
	entities.push_back(tree);
	tree->GetTransform()->MoveAbsolute(12, -5, 0);
	tree->GetTransform()->Scale(0.25f, 0.25f, 0.25f);

	// Create a flat surface in the center
	GameEntity* box = new GameEntity(assets.GetMesh("Models\\cube.obj"), solidShinyPlastic);
	entities.push_back(box);
	box->GetTransform()->MoveAbsolute(0, 0, 0);
	box->GetTransform()->Scale(15, 0.1f, 3);


	// Transform test =====================================
	entities[0]->GetTransform()->AddChild(entities[1]->GetTransform(), true);


	// Refractive setup ===================================
	cobbleMat2xPBR->SetRefractive(true);
	bronzeMatPBR->SetRefractive(true);
	treeMat->SetRefractive(true);
}


// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
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
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Prepare to resize the window by having the renderer release
	// its references to the back buffers, which is necessary
	// before the swap chain can actually resize those buffers
	renderer->PreResize();

	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update the renderer
	renderer->PostResize(width, height, backBufferRTV, depthStencilView);

	// Update our projection matrix to match the new aspect ratio
	if (camera)
		camera->UpdateProjectionMatrix(this->width / (float)this->height);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Get the input instance once
	Input& input = Input::GetInstance();

	// Update the camera
	camera->Update(deltaTime);

	// Move an object
	entities[0]->GetTransform()->Rotate(0, deltaTime, 0);
	float scale = 2.0f + sin(totalTime) / 2.0f;
	entities[0]->GetTransform()->SetScale(scale, scale, scale);

	// Parent/unparent for testing
	if (input.KeyPress('P')) entities[0]->GetTransform()->AddChild(entities[1]->GetTransform());
	if (input.KeyPress('U')) entities[0]->GetTransform()->RemoveChild(entities[1]->GetTransform());

	// Create the UI during update!
	CreateUI(deltaTime);

	// Check various keys
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();
}


void Game::CreateUI(float dt)
{
	// IMGUI
	{
		// Reset input manager's gui state
		// so we don't taint our own input
		Input& input = Input::GetInstance();
		input.SetGuiKeyboardCapture(false);
		input.SetGuiMouseCapture(false);

		// Set io info
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = dt;
		io.DisplaySize.x = (float)this->width;
		io.DisplaySize.y = (float)this->height;
		io.KeyCtrl = input.KeyDown(VK_CONTROL);
		io.KeyShift = input.KeyDown(VK_SHIFT);
		io.KeyAlt = input.KeyDown(VK_MENU);
		io.MousePos.x = (float)input.GetMouseX();
		io.MousePos.y = (float)input.GetMouseY();
		io.MouseDown[0] = input.MouseLeftDown();
		io.MouseDown[1] = input.MouseRightDown();
		io.MouseDown[2] = input.MouseMiddleDown();
		io.MouseWheel = input.GetMouseWheel();
		input.GetKeyArray(io.KeysDown, 256);

		// Reset the frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Determine new input capture
		input.SetGuiKeyboardCapture(io.WantCaptureKeyboard);
		input.SetGuiMouseCapture(io.WantCaptureMouse);
	}

	// Combined into a single window
	ImGui::Begin("Debug");

	// Showing the demo window?
	{
		static bool showDemoWindow = false;
		if (ImGui::Button("Show Demo Window"))
			showDemoWindow = !showDemoWindow;

		if (showDemoWindow)
			ImGui::ShowDemoWindow();
	}

	// Toggle point lights
	{
		ImGui::SameLine();

		bool visible = renderer->GetPointLightsVisible();
		if (ImGui::Button(visible ? "Hide Lights" : "Show Lights"))
			renderer->SetPointLightsVisible(!visible);
	}

	// All entity transforms
	if (ImGui::CollapsingHeader("Lights"))
	{
		int lightCount = (int)renderer->GetActiveLightCount();
		if (ImGui::SliderInt("Light Count", &lightCount, 0, MAX_LIGHTS))
			renderer->SetActiveLightCount((unsigned int)lightCount);

		while (lightCount >= lights.size())
		{
			Light light = {};
			lights.push_back(light);
		}

		for (int i = 0; i < lightCount; i++)
		{
			UILight(lights[i], i);
		}
	}

	// All scene entities
	if (ImGui::CollapsingHeader("Entities"))
	{
		if (ImGui::CollapsingHeader("Set All Materials To..."))
		{
			for (int i = 0; i < materials.size(); i++)
			{
				std::string label = "Material " + std::to_string(i);
				if (ImGui::Button(label.c_str()))
				{
					for (auto e : entities) e->SetMaterial(materials[i]);
				}
			}
		}

		for (int i = 0; i < entities.size(); i++)
		{
			UIEntity(entities[i], i);
		}
	}

	// SSAO Options
	if (ImGui::CollapsingHeader("SSAO Options"))
	{
		ImVec2 size = ImGui::GetItemRectSize();
		float rtHeight = size.x * ((float)height / width);

		bool ssao = renderer->GetSSAOEnabled();
		if (ImGui::Button(ssao ? "SSAO Enabled" : "SSAO Disabled"))
			renderer->SetSSAOEnabled(!ssao);

		ImGui::SameLine();
		bool ssaoOnly = renderer->GetSSAOOutputOnly();
		if (ImGui::Button("SSAO Output Only"))
			renderer->SetSSAOOutputOnly(!ssaoOnly);

		int ssaoSamples = renderer->GetSSAOSamples();
		if (ImGui::SliderInt("SSAO Samples", &ssaoSamples, 1, 64))
			renderer->SetSSAOSamples(ssaoSamples);

		float ssaoRadius = renderer->GetSSAORadius();
		if (ImGui::SliderFloat("SSAO Sample Radius", &ssaoRadius, 0.0f, 2.0f))
			renderer->SetSSAORadius(ssaoRadius);

		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::SSAO_RESULTS).Get(), ImVec2(size.x, rtHeight));
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::SSAO_BLUR).Get(), ImVec2(size.x, rtHeight));
	}

	if (ImGui::CollapsingHeader("All Render Targets"))
	{
		ImVec2 size = ImGui::GetItemRectSize();
		float rtHeight = size.x * ((float)height / width);

		for (int i = 0; i < RenderTargetType::RENDER_TARGET_TYPE_COUNT; i++)
		{
			ImageWithHover(renderer->GetRenderTargetSRV((RenderTargetType)i).Get(), ImVec2(size.x, rtHeight));
		}

		ImageWithHover(Assets::GetInstance().GetTexture("random").Get(), ImVec2(256, 256));
	}

	ImGui::End();
}

void Game::UIEntity(GameEntity* entity, int index)
{
	std::string indexStr = std::to_string(index);

	std::string nodeName = "Entity " + indexStr;
	if (ImGui::TreeNode(nodeName.c_str()))
	{
		// Transform -----------------------
		if (ImGui::CollapsingHeader("Transform"))
		{
			Transform* transform = entity->GetTransform();
			XMFLOAT3 pos = transform->GetPosition();
			XMFLOAT3 rot = transform->GetPitchYawRoll();
			XMFLOAT3 scale = transform->GetScale();

			if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
			{
				transform->SetPosition(pos.x, pos.y, pos.z);
			}

			if (ImGui::DragFloat3("Pitch/Yaw/Roll", &rot.x, 0.1f))
			{
				transform->SetRotation(rot.x, rot.y, rot.z);
			}

			if (ImGui::DragFloat3("Scale", &scale.x, 0.1f, 0.0f))
			{
				transform->SetScale(scale.x, scale.y, scale.z);
			}
		}

		// Material ------------------------
		if (ImGui::CollapsingHeader("Material"))
		{
			std::string comboID = "Material##" + indexStr;

			// Need the material index to preview the name
			// (Ugh, gross O(n) search over and over)
			int index = (int)std::distance(materials.begin(), std::find(materials.begin(), materials.end(), entity->GetMaterial()));
			std::string previewName = "Material " + std::to_string(index);

			// Start the material drop down box
			if (ImGui::BeginCombo(comboID.c_str(), previewName.c_str()))
			{
				// Show all materials
				for (int i = 0; i < materials.size(); i++)
				{
					// Is this one selected?
					bool selected = (entity->GetMaterial() == materials[i]);

					// Create the entry
					std::string matName = "Material " + std::to_string(i);
					if (ImGui::Selectable(matName.c_str(), selected))
						entity->SetMaterial(materials[i]);

					if (selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
		}

		ImGui::TreePop();
	}

}

void Game::UILight(Light& light, int index)
{
	std::string indexStr = std::to_string(index);

	std::string nodeName = "Light " + indexStr;
	if (ImGui::TreeNode(nodeName.c_str()))
	{
		std::string radioDirID = "Directional##" + indexStr;
		std::string radioPointID = "Point##" + indexStr;
		std::string radioSpotID = "Spot##" + indexStr;

		if (ImGui::RadioButton(radioDirID.c_str(), light.Type == LIGHT_TYPE_DIRECTIONAL))
		{
			light.Type = LIGHT_TYPE_DIRECTIONAL;
		}
		ImGui::SameLine();

		if (ImGui::RadioButton(radioPointID.c_str(), light.Type == LIGHT_TYPE_POINT))
		{
			light.Type = LIGHT_TYPE_POINT;
		}
		ImGui::SameLine();

		if (ImGui::RadioButton(radioSpotID.c_str(), light.Type == LIGHT_TYPE_SPOT))
		{
			light.Type = LIGHT_TYPE_SPOT;
		}

		// Direction
		if (light.Type == LIGHT_TYPE_DIRECTIONAL || light.Type == LIGHT_TYPE_SPOT)
		{
			std::string dirID = "Direction##" + indexStr;
			ImGui::DragFloat3(dirID.c_str(), &light.Direction.x, 0.1f);

			// Normalize the direction
			XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));
			XMStoreFloat3(&light.Direction, dirNorm);
		}

		// Position & Range
		if (light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT)
		{
			std::string posID = "Position##" + indexStr;
			ImGui::DragFloat3(posID.c_str(), &light.Position.x, 0.1f);


			std::string rangeID = "Range##" + indexStr;
			ImGui::SliderFloat(rangeID.c_str(), &light.Range, 0.1f, 100.0f);
		}

		// Spot falloff
		if (light.Type == LIGHT_TYPE_SPOT)
		{
			std::string spotFalloffID = "Spot Falloff##" + indexStr;
			ImGui::SliderFloat(spotFalloffID.c_str(), &light.SpotFalloff, 0.1f, 128.0f);
		}

		std::string buttonID = "Color##" + indexStr;
		ImGui::ColorEdit3(buttonID.c_str(), &light.Color.x);

		std::string intenseID = "Intensity##" + indexStr;
		ImGui::SliderFloat(intenseID.c_str(), &light.Intensity, 0.0f, 10.0f);

		ImGui::TreePop();
	}
}

void Game::ImageWithHover(ImTextureID user_texture_id, const ImVec2& size)
{
	// Draw the image
	ImGui::Image(user_texture_id, size);
	
	// Check for hover
	if (ImGui::IsItemHovered())
	{
		// Zoom amount and aspect of the image
		float zoom = 0.03f;
		float aspect = (float)size.x / size.y;

		// Get the coords of the image
		ImVec2 topLeft = ImGui::GetItemRectMin();
		ImVec2 bottomRight = ImGui::GetItemRectMax();
		
		// Get the mouse pos as a percent across the image, clamping near the edge
		ImVec2 mousePosGlobal = ImGui::GetMousePos();
		ImVec2 mousePos = ImVec2(mousePosGlobal.x - topLeft.x, mousePosGlobal.y - topLeft.y);
		ImVec2 uvPercent = ImVec2(mousePos.x / size.x, mousePos.y / size.y);

		uvPercent.x = max(uvPercent.x, zoom / 2);
		uvPercent.x = min(uvPercent.x, 1 - zoom / 2);
		uvPercent.y = max(uvPercent.y, zoom / 2 * aspect);
		uvPercent.y = min(uvPercent.y, 1 - zoom / 2 * aspect);

		// Figure out the uv coords for the zoomed image
		ImVec2 uvTL = ImVec2(uvPercent.x - zoom / 2, uvPercent.y - zoom / 2 * aspect);
		ImVec2 uvBR = ImVec2(uvPercent.x + zoom / 2, uvPercent.y + zoom / 2 * aspect);

		// Draw a floating box with a zoomed view of the image
		ImGui::BeginTooltip();
		ImGui::Image(user_texture_id, ImVec2(256,256), uvTL, uvBR);
		ImGui::EndTooltip();
	}
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	renderer->Render(camera);
}
