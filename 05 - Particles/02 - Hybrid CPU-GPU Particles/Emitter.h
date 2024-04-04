#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>

#include "SimpleShader.h"
#include "Camera.h"
#include "Material.h"
#include "Transform.h"


// Helper macro for getting a float between min and max
#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)

// We'll be mimicking this in HLSL
// so we need to care about alignment!
struct Particle
{
	float EmitTime;
	DirectX::XMFLOAT3 StartPosition;

	DirectX::XMFLOAT3 StartVelocity;
	float StartRotation;

	float EndRotation;
	DirectX::XMFLOAT3 pad;
};

class Emitter
{
public:
	Emitter(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		std::shared_ptr<Material> material,
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize = 1.0f,
		float endSize = 1.0f,
		bool constrainYAxis = false,
		DirectX::XMFLOAT4 startColor = DirectX::XMFLOAT4(1,1,1,1),
		DirectX::XMFLOAT4 endColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT3 emitterPosition = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 positionRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT2 rotationStartMinMax = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 rotationEndMinMax = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT3 startVelocity = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 velocityRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 emitterAcceleration = DirectX::XMFLOAT3(0, 0, 0),
		unsigned int spriteSheetWidth = 1,
		unsigned int spriteSheetHeight = 1,
		float spriteSheetSpeedScale = 1.0f
	);
	~Emitter();

	void Update(float dt, float currentTime);
	void Draw(
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		std::shared_ptr<Camera> camera,
		float currentTime);

	// Lifetime and emission
	float lifetime;
	int GetParticlesPerSecond();
	void SetParticlesPerSecond(int particlesPerSecond);
	int GetMaxParticles();
	void SetMaxParticles(int maxParticles);

	// Emitter-level data (this is the same for all particles)
	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 startVelocity;

	// Particle visual data (interpolated
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;
	bool constrainYAxis;

	// Particle randomization ranges
	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT2 rotationStartMinMax;
	DirectX::XMFLOAT2 rotationEndMinMax;

	// Sprite sheet animation
	float spriteSheetSpeedScale;
	bool IsSpriteSheet();

	Transform* GetTransform();
	std::shared_ptr<Material> GetMaterial();
	void SetMaterial(std::shared_ptr<Material> material);
private:
	// Emission
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;

	// Array of particle data
	Particle* particles;
	int maxParticles;

	// Lifetime tracking
	int indexFirstDead;
	int indexFirstAlive;
	int livingParticleCount;

	// Sprite sheet options
	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;
	
	// Rendering
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	
	// Material & transform
	Transform transform;
	std::shared_ptr<Material> material;

	// Creation and copy methods
	void CreateParticlesAndGPUResources();
	void CopyParticlesToGPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	// Simulation methods
	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};

