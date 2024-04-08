#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"
#include "PathHelpers.h"

#include "WICTextureLoader.h"

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
		hInstance,			// The application's handle
		L"DirectX Game",	// Text for the window's title bar (as a wide-character string)
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		false,				// Sync the framerate to the monitor refresh? (lock framerate)
		true),				// Show extra stats (fps) in title bar?
	ambientColor(0, 0, 0), // Ambient is zero'd out since it's not physically-based
	lightCount(3)
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
	// Call delete or delete[] on any objects or arrays you've
	// created using new or new[] within this class
	// - Note: this is unnecessary if using smart pointers

	// Call Release() on any Direct3D objects made within this class
	// - Note: this is unnecessary for D3D objects stored in ComPtrs

	// Deleting the singleton reference we've set up here
	delete& Assets::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Seed random
	srand((unsigned int)time(0));

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 3;
	GenerateLights();
	
	// Set initial graphics API state
	//  - These settings persist until we change them
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Create the camera
	camera = std::make_shared<Camera>(
		0.0f, 1.0f, -15.0f, // Position
		5.0f,				// Move speed
		0.002f,				// Look speed
		XM_PIDIV4,			// Field of view
		(float)windowWidth / windowHeight,  // Aspect ratio
		0.01f,				// Near clip
		100.0f,				// Far clip
		CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Initialize the asset manager and set up for on-demand loading
	Assets& assets = Assets::GetInstance();
	assets.Initialize(L"../../../../Assets/", L"./", device, context, true, true);

	// Set up sprite batch and sprite font
	spriteBatch = std::make_shared<SpriteBatch>(context.Get());

	// Create a sampler state for texture sampling options
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());


	// Create the sky (loading custom shaders in-line below)
	sky = std::make_shared<Sky>(
		FixPath(L"../../../../Assets/Skies/Night Moon/right.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/left.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/up.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/down.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/front.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/back.png").c_str(),
		assets.GetMesh(L"Models/cube"),
		assets.GetVertexShader(L"SkyVS"),
		assets.GetPixelShader(L"SkyPS"),
		sampler,
		device,
		context);


	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader(L"PixelShaderPBR");

	// Create basic materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/cobblestone_albedo"));
	cobbleMat2x->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/cobblestone_normals"));
	cobbleMat2x->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/cobblestone_roughness"));
	cobbleMat2x->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/cobblestone_metal"));

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/cobblestone_albedo"));
	cobbleMat4x->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/cobblestone_normals"));
	cobbleMat4x->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/cobblestone_roughness"));
	cobbleMat4x->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/cobblestone_metal"));

	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/floor_albedo"));
	floorMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/floor_normals"));
	floorMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/floor_roughness"));
	floorMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/floor_metal"));

	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/paint_albedo"));
	paintMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/paint_normals"));
	paintMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/paint_roughness"));
	paintMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/paint_metal"));

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/scratched_albedo"));
	scratchedMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/scratched_normals"));
	scratchedMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/scratched_roughness"));
	scratchedMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/scratched_metal"));

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/bronze_albedo"));
	bronzeMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/bronze_normals"));
	bronzeMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/bronze_roughness"));
	bronzeMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/bronze_metal"));

	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/rough_albedo"));
	roughMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/rough_normals"));
	roughMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/rough_roughness"));
	roughMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/rough_metal"));

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/wood_albedo"));
	woodMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/wood_normals"));
	woodMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/wood_roughness"));
	woodMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/wood_metal"));


	// === Create the scene ===
	std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(assets.GetMesh(L"Models/sphere"), scratchedMat);
	sphere->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(sphere);

	std::shared_ptr<GameEntity> helix = std::make_shared<GameEntity>(assets.GetMesh(L"Models/helix"), paintMat);
	entities.push_back(helix);

	std::shared_ptr<GameEntity> cube = std::make_shared<GameEntity>(assets.GetMesh(L"Models/cube"), woodMat);
	cube->GetTransform()->SetPosition(5, 0, 0);
	cube->GetTransform()->SetScale(2, 2, 2);
	entities.push_back(cube);

	// Particle states ====
	
	// A depth state for the particles
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Turns off depth writing
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&dsDesc, particleDepthState.GetAddressOf());

	// Blend for particles (additive)
	D3D11_BLEND_DESC blend = {};
	blend.AlphaToCoverageEnable = false;
	blend.IndependentBlendEnable = false;
	blend.RenderTarget[0].BlendEnable = true;
	blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // Still respect pixel shader output alpha
	blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blend, particleBlendState.GetAddressOf());

	// Debug rasterizer state for particles
	D3D11_RASTERIZER_DESC rd = {};
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = true;
	rd.FillMode = D3D11_FILL_WIREFRAME;
	device->CreateRasterizerState(&rd, particleDebugRasterState.GetAddressOf());


	// Grab loaded particle resources
	std::shared_ptr<SimpleVertexShader> particleVS = assets.GetVertexShader(L"ParticleVS");
	std::shared_ptr<SimplePixelShader> particlePS = assets.GetPixelShader(L"ParticlePS");

	// Create particle materials
	std::shared_ptr<Material> fireParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	fireParticle->AddSampler("BasicSampler", sampler);
	fireParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/fire_01"));

	std::shared_ptr<Material> twirlParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	twirlParticle->AddSampler("BasicSampler", sampler);
	twirlParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/twirl_02"));

	std::shared_ptr<Material> starParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	starParticle->AddSampler("BasicSampler", sampler);
	starParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/star_04"));

	std::shared_ptr<Material> animParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	animParticle->AddSampler("BasicSampler", sampler);
	animParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/flame_animated"));


	// Create example emitters
	
	// Flame thrower
	emitters.push_back(std::make_shared<Emitter>(
		160,							// Max particles
		30,								// Particles per second
		5.0f,							// Particle lifetime
		0.1f,							// Start size
		4.0f,							// End size
		XMFLOAT4(1, 0.1f, 0.1f, 0.7f),	// Start color
		XMFLOAT4(1, 0.6f, 0.1f, 0),		// End color
		XMFLOAT3(-2, 2, 0),				// Start velocity
		XMFLOAT3(0.2f, 0.2f, 0.2f),		// Velocity randomness range
		XMFLOAT3(2, 0, 0),				// Emitter position
		XMFLOAT3(0.1f, 0.1f, 0.1f),		// Position randomness range
		XMFLOAT4(-2, 2, -2, 2),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, -1, 0),				// Constant acceleration
		device,
		fireParticle));

	// Erratic swirly portal
	emitters.push_back(std::make_shared<Emitter>(
		45,								// Max particles
		20,								// Particles per second
		2.0f,							// Particle lifetime
		3.0f,							// Start size
		2.0f,							// End size
		XMFLOAT4(0.2f, 0.1f, 0.1f, 0.0f),// Start color
		XMFLOAT4(0.2f, 0.7f, 0.1f, 1.0f),// End color
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0, 0, 0),				// Velocity randomness range
		XMFLOAT3(3.5f, 3.5f, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),				// Position randomness range
		XMFLOAT4(-5, 5, -5, 5),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),				// Constant acceleration
		device,
		twirlParticle));

	// Falling star field
	emitters.push_back(std::make_shared<Emitter>(
		250,							// Max particles
		100,							// Particles per second
		2.0f,							// Particle lifetime
		2.0f,							// Start size
		0.0f,							// End size
		XMFLOAT4(0.1f, 0.2f, 0.5f, 0.0f),// Start color
		XMFLOAT4(0.1f, 0.1f, 0.3f, 3.0f),// End color (ending with high alpha so we hit 1.0 sooner)
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0.1f, 0, 0.1f),		// Velocity randomness range
		XMFLOAT3(-2.5f, -1, 0),			// Emitter position
		XMFLOAT3(1, 0, 1),				// Position randomness range
		XMFLOAT4(0, 0, -3, 3),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, -2, 0),				// Constant acceleration
		device,
		starParticle));

	// Animated fire texture
	emitters.push_back(std::make_shared<Emitter>(
		5,						// Max particles
		2,						// Particles per second
		2.0f,					// Particle lifetime
		2.0f,					// Start size
		2.0f,					// End size
		XMFLOAT4(1, 1, 1, 1),	// Start color
		XMFLOAT4(1, 1, 1, 0),	// End color
		XMFLOAT3(0, 0, 0),		// Start velocity
		XMFLOAT3(0, 0, 0),		// Velocity randomness range
		XMFLOAT3(2, -2, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),		// Position randomness range
		XMFLOAT4(-2, 2, -2, 2),	// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),		// Constant acceleration
		device,
		animParticle,
		true,
		8,
		8));



}


void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(1, 1, 1);
	dir1.Intensity = 1.0f;
	dir1.CastsShadows = 1; // 0 = false, 1 = true

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(1, 1, 1);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(1, 1, 1);
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

	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// In the event we need it below
	Assets& assets = Assets::GetInstance();

	// Example input checking: Quit if the escape key is pressed
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE))
		Quit();

	// Update the camera this frame
	camera->Update(deltaTime);

	// Since Init() takes a while, the first deltaTime
	// ends up being a massive number, which ends up emitting
	// a ton of particles.  Skipping the very first frame!
	static bool firstFrame = true; // Only ever initialized once due to static
	if (firstFrame) { deltaTime = 0.0f; firstFrame = false; }

	// Update all emitters
	for (auto& e : emitters)
	{
		e->Update(deltaTime);
	}

	// Handle light count changes, clamped appropriately
	if (input.KeyDown('R')) lightCount = 3;
	if (input.KeyDown(VK_UP)) lightCount++;
	if (input.KeyDown(VK_DOWN)) lightCount--;
	lightCount = max(1, min(MAX_LIGHTS, lightCount));

	// Move lights
	for (int i = 0; i < lightCount; i++)
	{
		// Only adjust point lights
		if (lights[i].Type == LIGHT_TYPE_POINT)
		{
			// Adjust either X or Z
			float lightAdjust = sin(totalTime + i) * 5;

			if (i % 2 == 0) lights[i].Position.x = lightAdjust;
			else			lights[i].Position.z = lightAdjust;
		}
	}

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Frame START
	// - These things should happen ONCE PER FRAME
	// - At the beginning of Game::Draw() before drawing *anything*
	{
		// Clear the back buffer (erases what's on the screen)
		const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
		context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);

		// Clear the depth buffer (resets per-pixel occlusion information)
		context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw all emitters
	DrawParticles();

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Present the back buffer to the user
		//  - Puts the results of what we've drawn onto the window
		//  - Without this, the user never sees anything
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Must re-bind buffers after presenting, as they become unbound
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
	}
}

void Game::DrawParticles()
{
	// Particle drawing =============
	{

		// Particle states
		context->OMSetBlendState(particleBlendState.Get(), 0, 0xffffffff);	// Additive blending
		context->OMSetDepthStencilState(particleDepthState.Get(), 0);		// No depth WRITING

		// Draw all of the emitters
		for (auto& e : emitters)
		{
			e->Draw(context, camera, false);
		}

		// Should we also draw them in wireframe?
		if (Input::GetInstance().KeyDown('C'))
		{
			context->RSSetState(particleDebugRasterState.Get());
			for (auto& e : emitters)
			{
				e->Draw(context, camera, true);
			}
		}

		// Reset to default states for next frame
		context->OMSetBlendState(0, 0, 0xffffffff);
		context->OMSetDepthStencilState(0, 0);
		context->RSSetState(0);
	}
}
