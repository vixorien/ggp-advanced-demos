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

	float Alive;
	DirectX::XMFLOAT3 StartVelocity;

	float StartRotation;
	float EndRotation;
	DirectX::XMFLOAT2 PAD2;
};

struct ParticleDrawData
{
	unsigned int index;
};

class Emitter
{
public:
	Emitter(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		std::shared_ptr<Material> material,
		std::shared_ptr<SimpleComputeShader> emitCS,
		std::shared_ptr<SimpleComputeShader> updateCS,
		std::shared_ptr<SimpleComputeShader> deadListInitCS,
		std::shared_ptr<SimpleComputeShader> copyDrawCountCS,
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
		float spriteSheetSpeedScale = 1.0f,
		bool paused = false,
		bool visible = true
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
	bool GetPaused();
	void SetPaused(bool paused);
	bool GetVisible();
	void SetVisible(bool visible);

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
	int maxParticles;
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;
	bool paused;
	bool visible;
	float totalEmitterTime;

	// Sprite sheet options
	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;
	
	// General API refs
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

	// Particle buffer views (UAV and SRV)
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particlePoolSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particlePoolUAV;

	// Dead list related buffers and views
	Microsoft::WRL::ComPtr<ID3D11Buffer> deadListCounterBuffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDeadUAV;

	// Drawing related buffers and views
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> drawArgsBuffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> drawArgsUAV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDrawSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDrawUAV;

	// Compute shaders
	std::shared_ptr<SimpleComputeShader> emitCS;
	std::shared_ptr<SimpleComputeShader> updateCS;
	std::shared_ptr<SimpleComputeShader> deadListInitCS;
	std::shared_ptr<SimpleComputeShader> copyDrawCountCS;
	
	// Material & transform
	Transform transform;
	std::shared_ptr<Material> material;

	// Creation and copy methods
	void CreateGPUResources();
	
};

