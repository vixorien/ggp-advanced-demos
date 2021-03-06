
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"

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

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str(), 0, srv.GetAddressOf())
#define LoadShader(type, file) new type(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str())


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
	for (auto& m : meshes) delete m;
	for (auto& s : shaders) delete s; 
	for (auto& m : materials) delete m;
	for (auto& e : entities) delete e;

	// Delete any one-off objects
	delete sky;
	delete camera;
	delete arial;
	delete spriteBatch;

	// Delete singletons
	delete& Input::GetInstance();

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

	// Set up lights initially
	lightCount = 64;
	GenerateLights();

	// Make our camera
	camera = new Camera(
		0, 0, -10,	// Position
		3.0f,		// Move speed
		1.0f,		// Mouse look
		this->width / (float)this->height); // Aspect ratio
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders using our succinct LoadShader() macro
	SimpleVertexShader* vertexShader	= LoadShader(SimpleVertexShader, L"VertexShader.cso");
	SimplePixelShader* pixelShader		= LoadShader(SimplePixelShader, L"PixelShader.cso");
	SimplePixelShader* pixelShaderPBR	= LoadShader(SimplePixelShader, L"PixelShaderPBR.cso");
	SimplePixelShader* solidColorPS		= LoadShader(SimplePixelShader, L"SolidColorPS.cso");
	
	SimpleVertexShader* skyVS = LoadShader(SimpleVertexShader, L"SkyVS.cso");
	SimplePixelShader* skyPS  = LoadShader(SimplePixelShader, L"SkyPS.cso");

	shaders.push_back(vertexShader);
	shaders.push_back(pixelShader);
	shaders.push_back(pixelShaderPBR);
	shaders.push_back(solidColorPS);
	shaders.push_back(skyVS);
	shaders.push_back(skyPS);

	// Set up the sprite batch and load the sprite font
	spriteBatch = new SpriteBatch(context.Get());
	arial = new SpriteFont(device.Get(), GetFullPathTo_Wide(L"../../../Assets/Textures/arial.spritefont").c_str());

	// Make the meshes
	Mesh* sphereMesh = new Mesh(GetFullPathTo("../../../Assets/Models/sphere.obj").c_str(), device);
	Mesh* helixMesh = new Mesh(GetFullPathTo("../../../Assets/Models/helix.obj").c_str(), device);
	Mesh* cubeMesh = new Mesh(GetFullPathTo("../../../Assets/Models/cube.obj").c_str(), device);
	Mesh* coneMesh = new Mesh(GetFullPathTo("../../../Assets/Models/cone.obj").c_str(), device);

	meshes.push_back(sphereMesh);
	meshes.push_back(helixMesh);
	meshes.push_back(cubeMesh);
	meshes.push_back(coneMesh);

	
	// Declare the textures we'll need
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA,  cobbleN,  cobbleR,  cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA,  floorN,  floorR,  floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA,  paintN,  paintR,  paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA,  scratchedN,  scratchedR,  scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA,  bronzeN,  bronzeR,  bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA,  roughN,  roughR,  roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA,  woodN,  woodR,  woodM;

	// Load the textures using our succinct LoadTexture() macro
	LoadTexture(L"../../../Assets/Textures/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../../Assets/Textures/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../../Assets/Textures/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../../Assets/Textures/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../../Assets/Textures/floor_albedo.png", floorA);
	LoadTexture(L"../../../Assets/Textures/floor_normals.png", floorN);
	LoadTexture(L"../../../Assets/Textures/floor_roughness.png", floorR);
	LoadTexture(L"../../../Assets/Textures/floor_metal.png", floorM);
	
	LoadTexture(L"../../../Assets/Textures/paint_albedo.png", paintA);
	LoadTexture(L"../../../Assets/Textures/paint_normals.png", paintN);
	LoadTexture(L"../../../Assets/Textures/paint_roughness.png", paintR);
	LoadTexture(L"../../../Assets/Textures/paint_metal.png", paintM);
	
	LoadTexture(L"../../../Assets/Textures/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../../Assets/Textures/scratched_normals.png", scratchedN);
	LoadTexture(L"../../../Assets/Textures/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../../Assets/Textures/scratched_metal.png", scratchedM);
	
	LoadTexture(L"../../../Assets/Textures/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../../Assets/Textures/bronze_normals.png", bronzeN);
	LoadTexture(L"../../../Assets/Textures/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../../Assets/Textures/bronze_metal.png", bronzeM);
	
	LoadTexture(L"../../../Assets/Textures/rough_albedo.png", roughA);
	LoadTexture(L"../../../Assets/Textures/rough_normals.png", roughN);
	LoadTexture(L"../../../Assets/Textures/rough_roughness.png", roughR);
	LoadTexture(L"../../../Assets/Textures/rough_metal.png", roughM);
	
	LoadTexture(L"../../../Assets/Textures/wood_albedo.png", woodA);
	LoadTexture(L"../../../Assets/Textures/wood_normals.png", woodN);
	LoadTexture(L"../../../Assets/Textures/wood_roughness.png", woodR);
	LoadTexture(L"../../../Assets/Textures/wood_metal.png", woodM);

	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());


	// Create the sky using a DDS cube map
	/*sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\SunnyCubeMap.dds").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context);*/

	// Create the sky using 6 images
	sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\right.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\left.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\up.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\down.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\front.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\..\\Assets\\Skies\\Night\\back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context);

	// Create basic materials
	Material* cobbleMat2x = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), cobbleA, cobbleN, cobbleR, cobbleM, samplerOptions);
	Material* floorMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), floorA, floorN, floorR, floorM, samplerOptions);
	Material* paintMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), paintA, paintN, paintR, paintM, samplerOptions);
	Material* scratchedMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), scratchedA, scratchedN, scratchedR, scratchedM, samplerOptions);
	Material* bronzeMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), bronzeA, bronzeN, bronzeR, bronzeM, samplerOptions);
	Material* roughMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), roughA, roughN, roughR, roughM, samplerOptions);
	Material* woodMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), woodA, woodN, woodR, woodM, samplerOptions);

	materials.push_back(cobbleMat2x);
	materials.push_back(floorMat);
	materials.push_back(paintMat);
	materials.push_back(scratchedMat);
	materials.push_back(bronzeMat);
	materials.push_back(roughMat);
	materials.push_back(woodMat);

	// Create PBR materials
	Material* cobbleMat2xPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), cobbleA, cobbleN, cobbleR, cobbleM, samplerOptions);
	Material* floorMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), floorA, floorN, floorR, floorM, samplerOptions);
	Material* paintMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), paintA, paintN, paintR, paintM, samplerOptions);
	Material* scratchedMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), scratchedA, scratchedN, scratchedR, scratchedM, samplerOptions);
	Material* bronzeMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), bronzeA, bronzeN, bronzeR, bronzeM, samplerOptions);
	Material* roughMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), roughA, roughN, roughR, roughM, samplerOptions);
	Material* woodMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), woodA, woodN, woodR, woodM, samplerOptions);

	materials.push_back(cobbleMat2xPBR);
	materials.push_back(floorMatPBR);
	materials.push_back(paintMatPBR);
	materials.push_back(scratchedMatPBR);
	materials.push_back(bronzeMatPBR);
	materials.push_back(roughMatPBR);
	materials.push_back(woodMatPBR);



	// === Create the PBR entities =====================================
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


	// Save assets needed for drawing point lights
	// (Since these are just copies of the pointers,
	//  we won't need to directly delete them as 
	//  the original pointers will be cleaned up)
	lightMesh = sphereMesh;
	lightVS = vertexShader;
	lightPS = solidColorPS;
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
	while (lights.size() < lightCount)
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
	// Handle base-level DX resize stuff
	DXCore::OnResize();

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

	//// Toggle point lights
	//{
	//	ImGui::SameLine();

	//	bool visible = renderer->GetPointLightsVisible();
	//	if (ImGui::Button(visible ? "Hide Lights" : "Show Lights"))
	//		renderer->SetPointLightsVisible(!visible);
	//}

	// All entity transforms
	if (ImGui::CollapsingHeader("Lights"))
	{
		if (ImGui::SliderInt("Light Count", &lightCount, 0, MAX_LIGHTS))

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
		if (ImGui::Button("Set All Materials To..."))
			ImGui::OpenPopup("SetAllMaterials");

		if (ImGui::BeginPopup("SetAllMaterials"))
		{
			for (int i = 0; i < materials.size(); i++)
			{
				std::string label = "Material " + std::to_string(i);
				if (ImGui::Selectable(label.c_str()))
				{
					for (auto e : entities) e->SetMaterial(materials[i]);
				}
			}
			ImGui::EndPopup();
		}

		for (int i = 0; i < entities.size(); i++)
		{
			UIEntity(entities[i], i);
		}
	}

	ImGui::End();
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
			ImVec2 size = ImGui::GetItemRectSize();
			ImVec2 finalSize = ImVec2(size.x, size.x);

			// Material changer
			std::string comboID = "Change Material##" + indexStr;

			// Need the material index to preview the name
			// (Ugh, gross O(n) search over and over)
			int index = (int)std::distance(materials.begin(), std::find(materials.begin(), materials.end(), entity->GetMaterial()));
			std::string previewName = "Material " + std::to_string(index);

			// Start the material drop down box
			ImGui::Spacing();
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


			ImGui::Text("Albedo");
			ImageWithHover(entity->GetMaterial()->GetAlbedoMap().Get(), finalSize);

			ImGui::Spacing();
			ImGui::Text("Normals");
			ImageWithHover(entity->GetMaterial()->GetNormalMap().Get(), finalSize);

			ImGui::Spacing();
			ImGui::Text("Roughness");
			ImageWithHover(entity->GetMaterial()->GetRoughnessMap().Get(), finalSize);

			ImGui::Spacing();
			ImGui::Text("Metal");
			ImageWithHover(entity->GetMaterial()->GetMetalMap().Get(), finalSize);

			
		}

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
		ImGui::Image(user_texture_id, ImVec2(256, 256), uvTL, uvBR);
		ImGui::EndTooltip();
	}
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);


	// Draw all of the entities
	for (auto ge : entities)
	{
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		SimplePixelShader* ps = ge->GetMaterial()->GetPS();
		ps->SetData("Lights", (void*)(&lights[0]), sizeof(Light) * lightCount);
		ps->SetInt("LightCount", lightCount);
		ps->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
		ps->CopyBufferData("perFrame");

		// Draw the entity
		ge->Draw(context, camera);
	}

	// Draw the light sources
	DrawPointLights();

	// Draw the sky
	sky->Draw(camera);

	// Draw some UI
	DrawUI();

	// IMGUI
	{
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}


// --------------------------------------------------------
// Draws the point lights as solid color spheres
// --------------------------------------------------------
void Game::DrawPointLights()
{
	// Turn on these shaders
	lightVS->SetShader();
	lightPS->SetShader();

	// Set up vertex shader
	lightVS->SetMatrix4x4("view", camera->GetView());
	lightVS->SetMatrix4x4("projection", camera->GetProjection());

	for (int i = 0; i < lightCount; i++)
	{
		Light light = lights[i];

		// Only drawing points, so skip others
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		// (assuming range is between 5 - 10)
		float scale = light.Range / 10.0f;

		// Make the transform for this light
		XMMATRIX rotMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
		XMMATRIX worldMat = scaleMat * rotMat * transMat;

		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTrans;
		XMStoreFloat4x4(&world, worldMat);
		XMStoreFloat4x4(&worldInvTrans, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		// Set up the world matrix for this light
		lightVS->SetMatrix4x4("world", world);
		lightVS->SetMatrix4x4("worldInverseTranspose", worldInvTrans);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		lightPS->SetFloat3("Color", finalColor);

		// Copy data
		lightVS->CopyAllBufferData();
		lightPS->CopyAllBufferData();

		// Draw
		lightMesh->SetBuffersAndDraw(context);
	}

}


// --------------------------------------------------------
// Draws a simple informational "UI" using sprite batch
// --------------------------------------------------------
void Game::DrawUI()
{
	spriteBatch->Begin();

	// Basic controls
	float h = 10.0f;
	arial->DrawString(spriteBatch, L"Controls:", XMVectorSet(10, h, 0, 0));
	arial->DrawString(spriteBatch, L" (WASD, X, Space) Move camera", XMVectorSet(10, h + 20, 0, 0));
	arial->DrawString(spriteBatch, L" (Left Click & Drag) Rotate camera", XMVectorSet(10, h + 40, 0, 0));
	arial->DrawString(spriteBatch, L" (Left Shift) Hold to speed up camera", XMVectorSet(10, h + 60, 0, 0));
	arial->DrawString(spriteBatch, L" (Left Ctrl) Hold to slow down camera", XMVectorSet(10, h + 80, 0, 0));
	arial->DrawString(spriteBatch, L" (TAB) Randomize lights", XMVectorSet(10, h + 100, 0, 0));

	// Current "scene" info
	h = 150;
	arial->DrawString(spriteBatch, L"Scene Details:", XMVectorSet(10, h, 0, 0));
	arial->DrawString(spriteBatch, L" Top: PBR materials", XMVectorSet(10, h + 20, 0, 0));
	arial->DrawString(spriteBatch, L" Bottom: Non-PBR materials", XMVectorSet(10, h + 40, 0, 0));

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}
