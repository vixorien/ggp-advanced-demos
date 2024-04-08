#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

#include "Camera.h"
#include "Material.h"
#include "Transform.h"
#include "SimpleShader.h"

struct Particle
{
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 StartPosition;
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 StartVelocity;
	float Size;
	float Age;
	float RotationStart;
	float RotationEnd;
	float Rotation;
};

struct ParticleVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 UV;
	DirectX::XMFLOAT4 Color;
};

class Emitter
{
public:
	Emitter(
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize,
		float endSize,
		DirectX::XMFLOAT4 startColor,
		DirectX::XMFLOAT4 endColor,
		DirectX::XMFLOAT3 startVelocity,
		DirectX::XMFLOAT3 velocityRandomRange,
		DirectX::XMFLOAT3 emitterPosition,
		DirectX::XMFLOAT3 positionRandomRange,
		DirectX::XMFLOAT4 rotationRandomRanges,
		DirectX::XMFLOAT3 emitterAcceleration,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		std::shared_ptr<Material> material,
		bool isSpriteSheet = false,
		unsigned int spriteSheetWidth = 1,
		unsigned int spriteSheetHeight = 1
	);
	~Emitter();

	void Update(float dt);
	void Draw(
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
		std::shared_ptr<Camera> camera,
		bool debugWireframe);

	Transform* GetTransform();
	std::shared_ptr<Material> GetMaterial();
	void SetMaterial(std::shared_ptr<Material> material);

private:
	// Emission properties
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceEmit;

	bool isSpriteSheet;
	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;

	int livingParticleCount;
	float lifetime;

	DirectX::XMFLOAT2 DefaultUVs[4];

	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 startVelocity;

	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT4 rotationRandomRanges; // Min start, max start, min end, max end

	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;

	// Particle array
	Particle* particles;
	int maxParticles;
	int firstDeadIndex;
	int firstAliveIndex;

	// Rendering
	ParticleVertex* localParticleVertices;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	// Material & transform
	Transform transform;
	std::shared_ptr<Material> material;

	// Update Methods
	void UpdateSingleParticle(float dt, int index);
	void SpawnParticle();

	// Copy methods
	void CopyParticlesToGPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera);
	void CopyOneParticle(int index, std::shared_ptr<Camera> camera);
	DirectX::XMFLOAT3 CalcParticleVertexPosition(int particleIndex, int quadCornerIndex, std::shared_ptr<Camera> camera);
};

