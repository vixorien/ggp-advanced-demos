
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"
#include "Helpers.h"

#include "WICTextureLoader.h"
#include "../../Common/ImGui/imgui.h"
#include "../../Common/ImGui/imgui_impl_dx11.h"
#include "../../Common/ImGui/imgui_impl_win32.h"


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(file).c_str(), 0, srv.GetAddressOf())
#define LoadShader(type, file) std::make_shared<type>(device.Get(), context.Get(), FixPath(file).c_str())


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
	lightCount(0),
	showUIDemoWindow(false),
	useOptimizedRendering(false)
{
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

	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// Deleting the singleton reference we've set up here
	delete& Assets::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
	ImGui::StyleColorsDark();

	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();

	// Set up lights initially
	lightCount = 64;
	GenerateLights();

	// Set initial graphics API state
	//  - These settings persist until we change them
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Set up the renderer
	renderer = std::make_shared<Renderer>(
		device, context, swapChain,
		windowWidth, windowHeight,
		backBufferRTV, depthBufferDSV);
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Initialize the asset manager and set it up to load assets on demand
	// Note: You could call LoadAllAssets() to load literally everything
	//       found in the asstes folder, but "load on demand" is more efficient
	Assets& assets = Assets::GetInstance();
	assets.Initialize(L"../../../../Assets/", L"./", device, context, true, true);

	// Load a scene json file
	scene = Scene::Load(FixPath(L"../../../../Assets/Scenes/pbrSpheres.scene"), device, context);
	scene->GetCurrentCamera()->UpdateProjectionMatrix(this->windowWidth / (float)this->windowHeight);

	// Create textures, simple PBR materials & entities (mostly for IBL testing)
	assets.CreateSolidColorTexture(L"white", 2, 2, XMFLOAT4(1, 1, 1, 1));
	assets.CreateSolidColorTexture(L"black", 2, 2, XMFLOAT4(0, 0, 0, 0));
	assets.CreateSolidColorTexture(L"grey", 2, 2, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));
	assets.CreateSolidColorTexture(L"darkGrey", 2, 2, XMFLOAT4(0.25f, 0.25f, 0.25f, 1));
	assets.CreateSolidColorTexture(L"flatNormalMap", 2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f));

	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> psPBR = assets.GetPixelShader(L"PixelShaderPBR");

	std::shared_ptr<Material> solidShinyMetal = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidShinyMetal->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidShinyMetal->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidShinyMetal->AddTextureSRV("RoughnessMap", assets.GetTexture(L"black"));
	solidShinyMetal->AddTextureSRV("MetalMap", assets.GetTexture(L"white"));
	solidShinyMetal->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidShinyMetal->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));

	std::shared_ptr<Material> solidQuarterRoughMetal = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidQuarterRoughMetal->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidQuarterRoughMetal->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidQuarterRoughMetal->AddTextureSRV("RoughnessMap", assets.GetTexture(L"darkGrey"));
	solidQuarterRoughMetal->AddTextureSRV("MetalMap", assets.GetTexture(L"white"));
	solidQuarterRoughMetal->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidQuarterRoughMetal->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));

	std::shared_ptr<Material> solidHalfRoughMetal = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidHalfRoughMetal->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidHalfRoughMetal->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidHalfRoughMetal->AddTextureSRV("RoughnessMap", assets.GetTexture(L"grey"));
	solidHalfRoughMetal->AddTextureSRV("MetalMap", assets.GetTexture(L"white"));
	solidHalfRoughMetal->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidHalfRoughMetal->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));

	std::shared_ptr<Material> solidShinyPlastic = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidShinyPlastic->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidShinyPlastic->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidShinyPlastic->AddTextureSRV("RoughnessMap", assets.GetTexture(L"black"));
	solidShinyPlastic->AddTextureSRV("MetalMap", assets.GetTexture(L"black"));
	solidShinyPlastic->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidShinyPlastic->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));

	std::shared_ptr<Material> solidQuarterRoughPlastic = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidQuarterRoughPlastic->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidQuarterRoughPlastic->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidQuarterRoughPlastic->AddTextureSRV("RoughnessMap", assets.GetTexture(L"darkGrey"));
	solidQuarterRoughPlastic->AddTextureSRV("MetalMap", assets.GetTexture(L"black"));
	solidQuarterRoughPlastic->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidQuarterRoughPlastic->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));

	std::shared_ptr<Material> solidHalfRoughPlastic = std::make_shared<Material>(psPBR, vs, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	solidHalfRoughPlastic->AddTextureSRV("Albedo", assets.GetTexture(L"white"));
	solidHalfRoughPlastic->AddTextureSRV("NormalMap", assets.GetTexture(L"flatNormalMap"));
	solidHalfRoughPlastic->AddTextureSRV("RoughnessMap", assets.GetTexture(L"grey"));
	solidHalfRoughPlastic->AddTextureSRV("MetalMap", assets.GetTexture(L"black"));
	solidHalfRoughPlastic->AddSampler("BasicSampler", assets.GetSampler(L"Samplers/anisotropic16Wrap"));
	solidHalfRoughPlastic->AddSampler("ClampSampler", assets.GetSampler(L"Samplers/anisotropic16Clamp"));


	std::shared_ptr<Mesh> sphereMesh = assets.GetMesh(L"Models/sphere");
	std::shared_ptr<GameEntity> shinyMetal = std::make_shared<GameEntity>(sphereMesh, solidShinyMetal);
	shinyMetal->GetTransform()->SetPosition(-6, -1, 0);
	shinyMetal->GetTransform()->SetScale(2);
	scene->AddEntity(shinyMetal);

	std::shared_ptr<GameEntity> quarterRoughMetal = std::make_shared<GameEntity>(sphereMesh, solidQuarterRoughMetal);
	quarterRoughMetal->GetTransform()->SetPosition(-4, -1, 0);
	quarterRoughMetal->GetTransform()->SetScale(2);
	scene->AddEntity(quarterRoughMetal);

	std::shared_ptr<GameEntity> roughMetal = std::make_shared<GameEntity>(sphereMesh, solidHalfRoughMetal);
	roughMetal->GetTransform()->SetPosition(-2, -1, 0);
	roughMetal->GetTransform()->SetScale(2);
	scene->AddEntity(roughMetal);

	std::shared_ptr<GameEntity> shinyPlastic = std::make_shared<GameEntity>(sphereMesh, solidShinyPlastic);
	shinyPlastic->GetTransform()->SetPosition(2, -1, 0);
	shinyPlastic->GetTransform()->SetScale(2);
	scene->AddEntity(shinyPlastic);

	std::shared_ptr<GameEntity> quarterRoughPlastic = std::make_shared<GameEntity>(sphereMesh, solidQuarterRoughPlastic);
	quarterRoughPlastic->GetTransform()->SetPosition(4, -1, 0);
	quarterRoughPlastic->GetTransform()->SetScale(2);
	scene->AddEntity(quarterRoughPlastic);

	std::shared_ptr<GameEntity> roughPlastic = std::make_shared<GameEntity>(sphereMesh, solidHalfRoughPlastic);
	roughPlastic->GetTransform()->SetPosition(6, -1, 0);
	roughPlastic->GetTransform()->SetScale(2);
	scene->AddEntity(roughPlastic);
}


// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Create extra lights
	while (scene->GetLights().size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		scene->AddLight(point);
	}

}

void Game::AddRandomEntity()
{
	// Get the materail from a random entity in the scene
	int entityIndex = rand() % scene->GetEntities().size();
	std::shared_ptr<Material> mat = scene->GetEntities()[entityIndex]->GetMaterial();

	// Choose a random mesh
	std::shared_ptr<Mesh> mesh;
	Assets& assets = Assets::GetInstance();

	int meshIndex = rand() % 5;
	switch (meshIndex)
	{
	case 0: mesh = assets.GetMesh(L"Models/cube"); break;
	case 1: mesh = assets.GetMesh(L"Models/sphere"); break;
	case 2: mesh = assets.GetMesh(L"Models/helix"); break;
	case 3: mesh = assets.GetMesh(L"Models/torus"); break;
	case 4: mesh = assets.GetMesh(L"Models/cylinder"); break;
	}

	// Create the entity
	std::shared_ptr<GameEntity> ge = std::make_shared<GameEntity>(mesh, mat);
	float range = 20;
	ge->GetTransform()->SetPosition(RandomRange(-range, range), RandomRange(-range, range), RandomRange(-range, range));
	ge->GetTransform()->SetScale(RandomRange(0.5f, 3.0f));

	// Add to scene
	scene->AddEntity(ge);
}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	renderer->PreResize();
	DXCore::OnResize();
	renderer->PostResize(windowWidth, windowHeight, backBufferRTV, depthBufferDSV);

	// Update our projection matrix to match the new aspect ratio
	for (auto& c : scene->GetCameras())
		c->UpdateProjectionMatrix((float)windowWidth / windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Set up the new frame for the UI, then build
	// this frame's interface.  Note that the building
	// of the UI could happen at any point during update.
	UINewFrame(deltaTime);
	BuildUI();

	// Update the camera
	scene->GetCurrentCamera()->Update(deltaTime);

	// Check individual input
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Clear and set up the new frame
	renderer->FrameStart();

	// Draw the scene
	if (useOptimizedRendering)
		renderer->RenderOptimized(scene, lightCount);
	else
		renderer->RenderSimple(scene, lightCount);

	// Finalize the frame
	renderer->FrameEnd(vsync || !deviceSupportsTearing || isFullscreen);
}


// --------------------------------------------------------
// Prepares a new frame for the UI, feeding it fresh
// input and time information for this new frame.
// --------------------------------------------------------
void Game::UINewFrame(float deltaTime)
{
	// Get a reference to our custom input manager
	Input& input = Input::GetInstance();

	// Reset input manager's gui state so we don’t
	// taint our own input
	input.SetKeyboardCapture(false);
	input.SetMouseCapture(false);

	// Feed fresh input data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->windowWidth;
	io.DisplaySize.y = (float)this->windowHeight;

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	input.SetKeyboardCapture(io.WantCaptureKeyboard);
	input.SetMouseCapture(io.WantCaptureMouse);
}


// --------------------------------------------------------
// Builds the UI for the current frame
// --------------------------------------------------------
void Game::BuildUI()
{
	// Should we show the built-in demo window?
	if (showUIDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}

	// Actually build our custom UI, starting with a window
	ImGui::Begin("Inspector");
	{
		// Set a specific amount of space for widget labels
		ImGui::PushItemWidth(-160); // Negative value sets label width

		// === Overall details ===
		if (ImGui::TreeNode("App Details"))
		{
			ImGui::Spacing();
			ImGui::Text("Frame rate: %f fps", ImGui::GetIO().Framerate);
			ImGui::Text("Window Client Size: %dx%d", windowWidth, windowHeight);

			ImGui::Spacing();
			ImGui::Text("Scene Details");
			ImGui::Text("Top Row:");    ImGui::SameLine(125); ImGui::Text("PBR Materials");
			ImGui::Text("Bottom Row:"); ImGui::SameLine(125); ImGui::Text("Non-PBR Materials");
	
			// Should we show the demo window?
			ImGui::Spacing();
			if (ImGui::Button(showUIDemoWindow ? "Hide ImGui Demo Window" : "Show ImGui Demo Window"))
				showUIDemoWindow = !showUIDemoWindow;

			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}
		
		// === Controls ===
		if (ImGui::TreeNode("Controls"))
		{
			ImGui::Spacing();
			ImGui::Text("(WASD, X, Space)");    ImGui::SameLine(175); ImGui::Text("Move camera");
			ImGui::Text("(Left Click & Drag)"); ImGui::SameLine(175); ImGui::Text("Rotate camera");
			ImGui::Text("(Left Shift)");        ImGui::SameLine(175); ImGui::Text("Hold to speed up camera");
			ImGui::Text("(Left Ctrl)");         ImGui::SameLine(175); ImGui::Text("Hold to slow down camera");
			ImGui::Text("(TAB)");               ImGui::SameLine(175); ImGui::Text("Randomize lights");
			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Camera details ===
		if (ImGui::TreeNode("Camera"))
		{
			// Show UI for current camera
			CameraUI(scene->GetCurrentCamera());

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Entities ===
		if (ImGui::TreeNode("Scene Entities"))
		{
			if (ImGui::Button("Add Random Entity"))
				AddRandomEntity();

			// Loop and show the details for each entity
			for (int i = 0; i < scene->GetEntities().size(); i++)
			{
				// New node for each entity
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Entity Node", "Entity %d", i))
				{
					// Build UI for one entity at a time
					EntityUI(scene->GetEntities()[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Lights ===
		if (ImGui::TreeNode("Lights"))
		{
			// Light details
			ImGui::Spacing();
			ImGui::SliderInt("Light Count", &lightCount, 0, MAX_LIGHTS);
			ImGui::Spacing();

			// Loop and show the details for each entity
			for (int i = 0; i < lightCount && i < scene->GetLights().size(); i++)
			{
				// Name of this light based on type
				std::string lightName = "Light %d";
				switch (scene->GetLights()[i].Type)
				{
				case LIGHT_TYPE_DIRECTIONAL: lightName += " (Directional)"; break;
				case LIGHT_TYPE_POINT: lightName += " (Point)"; break;
				case LIGHT_TYPE_SPOT: lightName += " (Spot)"; break;
				}

				// New node for each light
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Light Node", lightName.c_str(), i))
				{
					// Build UI for one entity at a time
					LightUI(scene->GetLights()[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// --- Renderer ---
		if (ImGui::TreeNode("Renderer"))
		{
			ImGui::Checkbox("Optimize Rendering", &useOptimizedRendering);
			ImGui::Checkbox("Indirect Lighting", &renderer->indirectLighting);

			// Finalize the tree node
			ImGui::TreePop();
		}
	}
	ImGui::End();
}


// --------------------------------------------------------
// Builds the UI for a single camera
// --------------------------------------------------------
void Game::CameraUI(std::shared_ptr<Camera> cam)
{
	ImGui::Spacing();

	// Transform details
	XMFLOAT3 pos = cam->GetTransform()->GetPosition();
	XMFLOAT3 rot = cam->GetTransform()->GetPitchYawRoll();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
		cam->GetTransform()->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f))
		cam->GetTransform()->SetRotation(rot);
	ImGui::Spacing();

	// Clip planes
	float nearClip = cam->GetNearClip();
	float farClip = cam->GetFarClip();
	if (ImGui::DragFloat("Near Clip Distance", &nearClip, 0.01f, 0.001f, 1.0f))
		cam->SetNearClip(nearClip);
	if (ImGui::DragFloat("Far Clip Distance", &farClip, 1.0f, 10.0f, 1000.0f))
		cam->SetFarClip(farClip);

	// Projection type
	CameraProjectionType projType = cam->GetProjectionType();
	int typeIndex = (int)projType;
	if (ImGui::Combo("Projection Type", &typeIndex, "Perspective\0Orthographic"))
	{
		projType = (CameraProjectionType)typeIndex;
		cam->SetProjectionType(projType);
	}

	// Projection details
	if (projType == CameraProjectionType::Perspective)
	{
		// Convert field of view to degrees for UI
		float fov = cam->GetFieldOfView() * 180.0f / XM_PI;
		if (ImGui::SliderFloat("Field of View (Degrees)", &fov, 0.01f, 180.0f))
			cam->SetFieldOfView(fov * XM_PI / 180.0f); // Back to radians
	}
	else if (projType == CameraProjectionType::Orthographic)
	{
		float wid = cam->GetOrthographicWidth();
		if (ImGui::SliderFloat("Orthographic Width", &wid, 1.0f, 10.0f))
			cam->SetOrthographicWidth(wid);
	}

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single entity
// --------------------------------------------------------
void Game::EntityUI(std::shared_ptr<GameEntity> entity)
{
	ImGui::Spacing();

	// Transform details
	Transform* trans = entity->GetTransform();
	XMFLOAT3 pos = trans->GetPosition();
	XMFLOAT3 rot = trans->GetPitchYawRoll();
	XMFLOAT3 sca = trans->GetScale();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) trans->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f)) trans->SetRotation(rot);
	if (ImGui::DragFloat3("Scale", &sca.x, 0.01f)) trans->SetScale(sca);

	// Mesh details
	ImGui::Spacing();
	ImGui::Text("Mesh Index Count: %d", entity->GetMesh()->GetIndexCount());

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single light
// --------------------------------------------------------
void Game::LightUI(Light& light)
{
	// Light type
	if (ImGui::RadioButton("Directional", light.Type == LIGHT_TYPE_DIRECTIONAL))
	{
		light.Type = LIGHT_TYPE_DIRECTIONAL;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Point", light.Type == LIGHT_TYPE_POINT))
	{
		light.Type = LIGHT_TYPE_POINT;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Spot", light.Type == LIGHT_TYPE_SPOT))
	{
		light.Type = LIGHT_TYPE_SPOT;
	}

	// Direction
	if (light.Type == LIGHT_TYPE_DIRECTIONAL || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Direction", &light.Direction.x, 0.1f);

		// Normalize the direction
		XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));
		XMStoreFloat3(&light.Direction, dirNorm);
	}

	// Position & Range
	if (light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Position", &light.Position.x, 0.1f);
		ImGui::SliderFloat("Range", &light.Range, 0.1f, 100.0f);
	}

	// Spot falloff
	if (light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::SliderFloat("Spot Falloff", &light.SpotFalloff, 0.1f, 128.0f);
	}

	// Color details
	ImGui::ColorEdit3("Color", &light.Color.x);
	ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
}


