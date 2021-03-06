
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
#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)


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
		true),			   // Show extra stats (fps) in title bar?
	sky{},
	renderer{},
	freezeLights(false),
	freezeEntities(false)
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
		64,
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
	SimplePixelShader* psPBR = assets.GetPixelShader("PixelShaderPBR.cso");

	// Create PBR materials
	Material* cobbleMat2xPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2));
	cobbleMat2xPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\cobblestone_albedo.png"));
	cobbleMat2xPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\cobblestone_normals.png"));
	cobbleMat2xPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\cobblestone_roughness.png"));
	cobbleMat2xPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\cobblestone_metal.png"));
	cobbleMat2xPBR->AddPSSampler("BasicSampler", samplerOptions);
	cobbleMat2xPBR->AddPSSampler("ClampSampler", clampSampler);

	Material* cobbleMat10xPBR = new Material(vs, psPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(10, 10));
	cobbleMat10xPBR->AddPSTextureSRV("AlbedoTexture", assets.GetTexture("Textures\\cobblestone_albedo.png"));
	cobbleMat10xPBR->AddPSTextureSRV("NormalTexture", assets.GetTexture("Textures\\cobblestone_normals.png"));
	cobbleMat10xPBR->AddPSTextureSRV("RoughnessTexture", assets.GetTexture("Textures\\cobblestone_roughness.png"));
	cobbleMat10xPBR->AddPSTextureSRV("MetalTexture", assets.GetTexture("Textures\\cobblestone_metal.png"));
	cobbleMat10xPBR->AddPSSampler("BasicSampler", samplerOptions);
	cobbleMat10xPBR->AddPSSampler("ClampSampler", clampSampler);

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
	materials.push_back(cobbleMat10xPBR);
	materials.push_back(floorMatPBR);
	materials.push_back(paintMatPBR);
	materials.push_back(scratchedMatPBR);
	materials.push_back(bronzeMatPBR);
	materials.push_back(roughMatPBR);
	materials.push_back(woodMatPBR);


	// === Create the PBR entities =====================================

	// Floor entity
	GameEntity* box = new GameEntity(assets.GetMesh("Models\\cube.obj"), cobbleMat10xPBR);
	entities.push_back(box);
	box->GetTransform()->MoveAbsolute(0, -7, 0);
	box->GetTransform()->Scale(50, 1, 50);

	// Create a set of random entities
	Mesh* meshSet[5] =
	{
		assets.GetMesh("Models\\cube.obj"),
		assets.GetMesh("Models\\sphere.obj"),
		assets.GetMesh("Models\\helix.obj"),
		assets.GetMesh("Models\\torus.obj"),
		assets.GetMesh("Models\\cylinder.obj")
	};

	Material* matSet[7] =
	{
		cobbleMat2xPBR,
		floorMatPBR,
		paintMatPBR,
		scratchedMatPBR,
		bronzeMatPBR,
		roughMatPBR,
		woodMatPBR
	};

	for (int i = 0; i < 50; i++)
	{
		// Choose random mesh and materials
		Mesh* randMesh = meshSet[(int)RandomRange(0, 5)];
		Material* randMat = matSet[(int)RandomRange(0, 7)];

		GameEntity* ge = new GameEntity(randMesh, randMat);
		float size = RandomRange(0.5f, 5.0f);
		ge->GetTransform()->SetScale(size, size, size);
		ge->GetTransform()->SetPosition(
			RandomRange(-25.0f, 25.0f),
			RandomRange(-5.0f, 5.0f),
			RandomRange(-25.0f, 25.0f));
		ge->GetTransform()->SetRotation(
			RandomRange(0, XM_PI * 2),
			RandomRange(0, XM_PI * 2),
			RandomRange(0, XM_PI * 2));

		entities.push_back(ge);
	}
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
		point.Position = XMFLOAT3(RandomRange(-25.0f, 25.0f), RandomRange(-5.0f, 5.0f), RandomRange(-25.0f, 25.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(2.0f, 10.0f);
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

	// Slowly rotate entities (aside from floor)
	for (int i = 1; i < entities.size() && !freezeEntities; i++)
	{
		float rot = deltaTime * 0.1f;
		switch (i % 4)
		{
		case 0:	entities[i]->GetTransform()->Rotate(rot, rot, rot); break;
		case 1:	entities[i]->GetTransform()->Rotate(rot, 0, 0); break;
		case 2:	entities[i]->GetTransform()->Rotate(0, rot, 0); break;
		case 3:	entities[i]->GetTransform()->Rotate(0, 0, rot); break;
		}
	}

	// Move lights
	for (int i = 0; i < lights.size() && !freezeLights; i++)
	{
		// Only adjust point lights
		if (lights[i].Type == LIGHT_TYPE_POINT)
		{
			// Adjust either X or Z
			float lightAdjust = sin(totalTime / 5.0f + i) * 25.0f;

			if (i % 2 == 0) lights[i].Position.x = lightAdjust;
			else			lights[i].Position.z = lightAdjust;
		}
	}
	
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

	// Toggle entity freeze
	{
		if (ImGui::Button(freezeEntities ? "Unfreeze Entities" : "Freeze Entities"))
			freezeEntities = !freezeEntities;
	}

	// Showing the demo window?
	{
		ImGui::SameLine();

		static bool showDemoWindow = false;
		if (ImGui::Button("Show Demo Window"))
			showDemoWindow = !showDemoWindow;

		if (showDemoWindow)
			ImGui::ShowDemoWindow();
	}

	// All entity transforms
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::Indent(10.0f);

		// Size for images
		ImVec2 size = ImGui::GetItemRectSize();
		float rtHeight = size.x * ((float)height / width);

		// Warning for debug mode
#ifdef _DEBUG
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
		ImGui::Text("(Run in RELEASE MODE for best forward/deferred performance)");
		ImGui::PopStyleColor();
#endif

		// What's the render path?
		RenderPath path = renderer->GetRenderPath();
		if (ImGui::Button(path == RenderPath::RENDER_PATH_FORWARD ? "Forward Rendering" : "Deferred Rendering"))
			renderer->SetRenderPath(path == RenderPath::RENDER_PATH_FORWARD ? RenderPath::RENDER_PATH_DEFERRED : RenderPath::RENDER_PATH_FORWARD);
		
		ImGui::SameLine();

		// Should lights move?
		if (ImGui::Button(freezeLights ? "Unfreeze Lights" : "Freeze Lights"))
			freezeLights = !freezeLights;

		// Should the lights be visible?
		bool visible = renderer->GetPointLightsVisible();
		if (ImGui::Button(visible ? "Light Sources: On" : "Light Sources: Off"))
			renderer->SetPointLightsVisible(!visible);

		// Show silhouettes?
		if (visible && path == RenderPath::RENDER_PATH_DEFERRED)
		{
			ImGui::SameLine();
			
			bool sil = renderer->GetDeferredSilhouettes();
			if (ImGui::Button(sil ? "Silhouettes: On" : "Silhouettes: Off"))
				renderer->SetDeferredSilhouettes(!sil);
		}
		else
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::Button("Silhouettes Disabled");
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
		}

		// IBL Intensity
		float intensity = renderer->GetIBLIntensity();
		if (ImGui::SliderFloat("IBL Intensity", &intensity, 0.0f, 10.0f))
			renderer->SetIBLIntensity(intensity);

		int lightCount = (int)renderer->GetActiveLightCount();
		if (ImGui::SliderInt("Light Count", &lightCount, 0, MAX_LIGHTS))
			renderer->SetActiveLightCount((unsigned int)lightCount);

		// Holds all lights
		if (ImGui::CollapsingHeader("Lights"))
		{
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

		// Deferred options ---------------
		if (ImGui::CollapsingHeader("GBuffer & Light Buffer"))
		{
			if (renderer->GetRenderPath() == RenderPath::RENDER_PATH_DEFERRED)
			{
				// Show GBUFFER and other debug options
				ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_ALBEDO).Get(), ImVec2(size.x, rtHeight), "GBuffer Albedo");
				ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_NORMALS).Get(), ImVec2(size.x, rtHeight), "GBuffer Normals");
				ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_DEPTH).Get(), ImVec2(size.x, rtHeight), "GBuffer Depth");
				ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_METAL_ROUGH).Get(), ImVec2(size.x, rtHeight), "GBuffer Metal & Roughness");
				ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::LIGHT_BUFFER).Get(), ImVec2(size.x, rtHeight), "Light Buffer");
			}
			else
			{
				ImGui::Text("Switch to Deferred Rendering to see GBuffer");
			}
		}

		ImGui::Indent(-10.0f);
	}

	// All scene entities
	if (ImGui::CollapsingHeader("Entities"))
	{
		ImGui::Indent(10.0f);
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
		ImGui::Indent(-10.0f);
	}

	// SSAO Options
	if (ImGui::CollapsingHeader("SSAO Options"))
	{
		ImGui::Indent(10.0f);
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

		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::SSAO_RESULTS).Get(), ImVec2(size.x, rtHeight), "SSAO Results");
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::SSAO_BLUR).Get(), ImVec2(size.x, rtHeight), "SSAO Blurred Results");
		ImGui::Indent(-10.0f);
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

	// Deferred window
	if (renderer->GetRenderPath() == RenderPath::RENDER_PATH_DEFERRED)
	{
		ImGui::Begin("GBuffer & Light Buffer", 0, ImGuiWindowFlags_AlwaysHorizontalScrollbar);

		float imgHeight = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
		float imgWidth = imgHeight * ((float)width / height);

		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_ALBEDO).Get(), ImVec2(imgWidth, imgHeight), "GBuffer Albedo"); ImGui::SameLine();
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_NORMALS).Get(), ImVec2(imgWidth, imgHeight), "GBuffer Normals"); ImGui::SameLine();
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_DEPTH).Get(), ImVec2(imgWidth, imgHeight), "GBuffer Depth"); ImGui::SameLine();
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::GBUFFER_METAL_ROUGH).Get(), ImVec2(imgWidth, imgHeight), "GBuffer Metal & Roughness"); ImGui::SameLine();
		ImageWithHover(renderer->GetRenderTargetSRV(RenderTargetType::LIGHT_BUFFER).Get(), ImVec2(imgWidth, imgHeight), "Light Buffer");

		ImGui::End();
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
	ImageWithHover(user_texture_id, size, "");
}

void Game::ImageWithHover(ImTextureID user_texture_id, const ImVec2& size, const char* name)
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
		if (strlen(name) > 0) 
			ImGui::Text(name);
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
