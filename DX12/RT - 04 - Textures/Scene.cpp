#include "Scene.h"
#include "Helpers.h"

#include "Mesh.h"
#include "GameEntity.h"
#include "Material.h"
#include "DX12Helper.h"

#include <DirectXMath.h>
#include <stdlib.h>

using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Initialize static members
bool Scene::exampleScenesCreated = false;
std::vector<std::shared_ptr<Scene>> Scene::exampleScenes;

Scene::Scene(std::string name) : name(name) { }
Scene::~Scene() { }

std::string Scene::GetName() { return name; }
unsigned int Scene::EntityCount() { return (unsigned int)entities.size(); }
std::vector<std::shared_ptr<GameEntity>> Scene::GetEntities() { return entities; }

void Scene::AddEntity(std::shared_ptr<GameEntity> entity)
{
	entities.push_back(entity);
}

std::shared_ptr<GameEntity> Scene::GetEntity(unsigned int index)
{
	if (index >= entities.size())
		return 0;

	return entities[index];
}








void Scene::UpdateScene(std::shared_ptr<Scene> scene, float deltaTime, float totalTime)
{
	// Check the scene name and handle each differently
	if (scene->GetName() == "Spheres")
	{
		std::vector<std::shared_ptr<GameEntity>> entities = scene->GetEntities();

		entities[1]->GetTransform()->Rotate(deltaTime * 0.5f, deltaTime * 0.5f, deltaTime * 0.5f);
		entities[6]->GetTransform()->Rotate(0, deltaTime * 0.25f, 0);

		// Skip the first few entities:
		// 0: floor
		// 1: torus
		// 2,3,4,5: transparent balls
		// 6: ball parent
		// 7-28: test balls
		int skip = 29;

		// Rotate entities (skip first two)
		float range = 20;
		for (int i = skip; i < entities.size(); i++)
		{
			XMFLOAT3 pos = entities[i]->GetTransform()->GetPosition();
			XMFLOAT3 rot = entities[i]->GetTransform()->GetPitchYawRoll();
			XMFLOAT3 sc = entities[i]->GetTransform()->GetScale();
			switch (i % 2)
			{
			case 0:
				pos.x = sin((totalTime + i) * (4 / range)) * range;
				rot.z = -pos.x / (sc.x * 0.5f);
				break;

			case 1:
				pos.z = sin((totalTime + i) * (4 / range)) * range;
				rot.x = pos.z / (sc.x * 0.5f);
				break;
			}
			entities[i]->GetTransform()->SetPosition(pos);
			entities[i]->GetTransform()->SetRotation(rot);
		}
	}

}




std::vector<std::shared_ptr<Scene>> Scene::CreateExampleScenes(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
{
	if (exampleScenesCreated)
		return exampleScenes;

	// === MESHES ==============================

	std::shared_ptr<Mesh> cube = std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/cube.obj").c_str());
	std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/sphere.obj").c_str());
	std::shared_ptr<Mesh> helix = std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/helix.obj").c_str());
	std::shared_ptr<Mesh> torus = std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/torus.obj").c_str());
	std::shared_ptr<Mesh> cylinder = std::make_shared<Mesh>(FixPath(L"../../../../Assets/Models/cylinder.obj").c_str());


	// === TEXTURES ==============================

	// Quick macro to simplify texture loading lines below
#define LoadTexture(x) DX12Helper::GetInstance().LoadTexture(FixPath(x).c_str())

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

	D3D12_CPU_DESCRIPTOR_HANDLE woodAlbedo = LoadTexture(L"../../../../Assets/Textures/wood_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodNormals = LoadTexture(L"../../../../Assets/Textures/wood_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodRoughness = LoadTexture(L"../../../../Assets/Textures/wood_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodMetal = LoadTexture(L"../../../../Assets/Textures/wood_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE floorAlbedo = LoadTexture(L"../../../../Assets/Textures/floor_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorNormals = LoadTexture(L"../../../../Assets/Textures/floor_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorRoughness = LoadTexture(L"../../../../Assets/Textures/floor_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorMetal = LoadTexture(L"../../../../Assets/Textures/floor_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE paintAlbedo = LoadTexture(L"../../../../Assets/Textures/paint_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintNormals = LoadTexture(L"../../../../Assets/Textures/paint_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintRoughness = LoadTexture(L"../../../../Assets/Textures/paint_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintMetal = LoadTexture(L"../../../../Assets/Textures/paint_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE ironAlbedo = LoadTexture(L"../../../../Assets/Textures/rough_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironNormals = LoadTexture(L"../../../../Assets/Textures/rough_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironRoughness = LoadTexture(L"../../../../Assets/Textures/rough_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironMetal = LoadTexture(L"../../../../Assets/Textures/rough_metal.png");

	// === MATERIALS ==============================

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> greyDiffuse = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.5f, 0.5f), MaterialType::Normal, 1.0f, 0.0f);
	std::shared_ptr<Material> darkGrey = std::make_shared<Material>(pipelineState, XMFLOAT3(0.25f, 0.25f, 0.25f), MaterialType::Normal, 0.0f, 1.0f);
	std::shared_ptr<Material> metal = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.6f, 0.7f), MaterialType::Normal, 0.0f, 1.0f);

	std::shared_ptr<Material> cobblestone = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> scratched = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> bronze = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> floor = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> paint = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> iron = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> wood = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));

	// Set up textures
	cobblestone->AddTexture(cobblestoneAlbedo, 0);
	cobblestone->AddTexture(cobblestoneNormals, 1);
	cobblestone->AddTexture(cobblestoneRoughness, 2);
	cobblestone->AddTexture(cobblestoneMetal, 3);
	cobblestone->FinalizeTextures();

	scratched->AddTexture(scratchedAlbedo, 0);
	scratched->AddTexture(scratchedNormals, 1);
	scratched->AddTexture(scratchedRoughness, 2);
	scratched->AddTexture(scratchedMetal, 3);
	scratched->FinalizeTextures();

	bronze->AddTexture(bronzeAlbedo, 0);
	bronze->AddTexture(bronzeNormals, 1);
	bronze->AddTexture(bronzeRoughness, 2);
	bronze->AddTexture(bronzeMetal, 3);
	bronze->FinalizeTextures();

	floor->AddTexture(floorAlbedo, 0);
	floor->AddTexture(floorNormals, 1);
	floor->AddTexture(floorRoughness, 2);
	floor->AddTexture(floorMetal, 3);
	floor->FinalizeTextures();

	paint->AddTexture(paintAlbedo, 0);
	paint->AddTexture(paintNormals, 1);
	paint->AddTexture(paintRoughness, 2);
	paint->AddTexture(paintMetal, 3);
	paint->FinalizeTextures();

	wood->AddTexture(woodAlbedo, 0);
	wood->AddTexture(woodNormals, 1);
	wood->AddTexture(woodRoughness, 2);
	wood->AddTexture(woodMetal, 3);
	wood->FinalizeTextures();

	iron->AddTexture(ironAlbedo, 0);
	iron->AddTexture(ironNormals, 1);
	iron->AddTexture(ironRoughness, 2);
	iron->AddTexture(ironMetal, 3);
	iron->FinalizeTextures();

	// Transparent
	std::shared_ptr<Material> glassWhite = std::make_shared<Material>(pipelineState, XMFLOAT3(1.0f, 1.0f, 1.0f), MaterialType::Transparent, 0.0f);
	std::shared_ptr<Material> glassRed = std::make_shared<Material>(pipelineState, XMFLOAT3(1.0f, 0.1f, 0.1f), MaterialType::Transparent, 0.0f);
	std::shared_ptr<Material> glassGreen = std::make_shared<Material>(pipelineState, XMFLOAT3(0.1f, 1.0f, 0.1f), MaterialType::Transparent, 0.0f);
	std::shared_ptr<Material> glassBlue = std::make_shared<Material>(pipelineState, XMFLOAT3(0.1f, 0.1f, 1.0f), MaterialType::Transparent, 0.0f);


	// === SCENES =======================================

	// Basic sphere scene
	std::shared_ptr<Scene> sphereScene = std::make_shared<Scene>("Spheres");
	{
		// Floor
		std::shared_ptr<GameEntity> ground = std::make_shared<GameEntity>(cube, wood);
		ground->GetTransform()->SetScale(100);
		ground->GetTransform()->SetPosition(0, -52, 0);
		sphereScene->AddEntity(ground);

		// Spinning torus
		std::shared_ptr<GameEntity> t = std::make_shared<GameEntity>(torus, metal);
		t->GetTransform()->SetScale(2);
		t->GetTransform()->SetPosition(0, 2, 0);
		sphereScene->AddEntity(t);

		// Four floating transparent spheres
		std::shared_ptr<GameEntity> glassSphereWhite = std::make_shared<GameEntity>(sphere, glassWhite);
		std::shared_ptr<GameEntity> glassSphereRed = std::make_shared<GameEntity>(sphere, glassRed);
		std::shared_ptr<GameEntity> glassSphereGreen = std::make_shared<GameEntity>(sphere, glassGreen);
		std::shared_ptr<GameEntity> glassSphereBlue = std::make_shared<GameEntity>(sphere, glassBlue);

		glassSphereWhite->GetTransform()->SetPosition(0, 1, -2);
		glassSphereRed->GetTransform()->SetPosition(2, 1, 0);
		glassSphereGreen->GetTransform()->SetPosition(0, 1, 2);
		glassSphereBlue->GetTransform()->SetPosition(-2, 1, 0);

		sphereScene->AddEntity(glassSphereWhite);
		sphereScene->AddEntity(glassSphereRed);
		sphereScene->AddEntity(glassSphereGreen);
		sphereScene->AddEntity(glassSphereBlue);

		std::shared_ptr<GameEntity> parent = std::make_shared<GameEntity>(cube, greyDiffuse);
		parent->GetTransform()->SetPosition(0, 2, 0);
		parent->GetTransform()->SetScale(0.4f);
		parent->GetTransform()->AddChild(glassSphereWhite->GetTransform());
		parent->GetTransform()->AddChild(glassSphereRed->GetTransform());
		parent->GetTransform()->AddChild(glassSphereGreen->GetTransform());
		parent->GetTransform()->AddChild(glassSphereBlue->GetTransform());
		sphereScene->AddEntity(parent);

		// Test spheres
		for (int i = 0; i <= 10; i++)
		{
			std::shared_ptr<Material> matMetal = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1), MaterialType::Normal, i * 0.1f, 1.0f);
			std::shared_ptr<Material> matPlast = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 0, 0), MaterialType::Normal, i * 0.1f, 0.0f);

			std::shared_ptr<GameEntity> entMetal = std::make_shared<GameEntity>(sphere, matMetal);
			std::shared_ptr<GameEntity> entPlast = std::make_shared<GameEntity>(sphere, matPlast);

			entMetal->GetTransform()->SetPosition((i - 5) * 1.1f, 11.1f, 0);
			entPlast->GetTransform()->SetPosition((i - 5) * 1.1f, 10, 0);

			sphereScene->AddEntity(entMetal);
			sphereScene->AddEntity(entPlast);
		}

		float range = 20;
		for (int i = 0; i < 50; i++)
		{
			// Random roughness either 0 or 1
			float rough = RandomRange(0.0f, 1.0f) > 0.5f ? 0.0f : 1.0f;
			float emissiveIntensity = RandomRange(1.0f, 2.0f);
			float metalness = RandomRange(0.0f, 1.0f) > 0.5f ? 0.0f : 1.0f;

			// Random chance to be emissive
			MaterialType type = MaterialType::Normal;
			/*if (RandomRange(0.0f, 1.0f) > 0.9f)
			{
				type = MaterialType::Emissive;
			}*/

			std::shared_ptr<Material> mat = std::make_shared<Material>(
				pipelineState,
				XMFLOAT3(
					RandomRange(0.0f, 1.0f),
					RandomRange(0.0f, 1.0f),
					RandomRange(0.0f, 1.0f)),
				type,
				rough,
				metalness,
				emissiveIntensity);

			// Randomly choose some others
			float randMat = RandomRange(0, 1);
			if (randMat > 0.95f) mat = bronze;
			else if (randMat > 0.9f) mat = cobblestone;
			else if (randMat > 0.85f) mat = scratched;
			else if (randMat > 0.8f) mat = wood;
			else if (randMat > 0.75f) mat = iron;
			else if (randMat > 0.7f) mat = paint;
			else if (randMat > 0.65f) mat = floor;

			std::shared_ptr<GameEntity> sphereEnt = std::make_shared<GameEntity>(sphere, mat);
			sphereScene->AddEntity(sphereEnt);

			float scale = RandomRange(0.5f, 3.5f);
			sphereEnt->GetTransform()->SetScale(scale);

			sphereEnt->GetTransform()->SetPosition(
				RandomRange(-range, range),
				-2 + scale / 2.0f,
				RandomRange(-range, range));

		}
	}
	exampleScenes.push_back(sphereScene);



	// Finalize
	exampleScenesCreated = true;
	return exampleScenes;
}
