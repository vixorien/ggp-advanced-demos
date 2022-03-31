#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>

#include "SimpleShader.h"
#include "Camera.h"


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
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		SimpleVertexShader* vs,
		SimplePixelShader* ps,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture,
		float startSize = 1.0f,
		float endSize = 1.0f,
		DirectX::XMFLOAT4 startColor = DirectX::XMFLOAT4(1,1,1,1),
		DirectX::XMFLOAT4 endColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT3 emitterPosition = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 positionRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 startVelocity = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 velocityRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 emitterAcceleration = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT2 rotationStartMinMax = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 rotationEndMinMax = DirectX::XMFLOAT2(0, 0),
		unsigned int spriteSheetWidth = 1,
		unsigned int spriteSheetHeight = 1,
		float spriteSheetSpeedScale = 1.0f
	);
	~Emitter();

	void Update(float dt, float currentTime);
	void Draw(Camera* camera, float currentTime);

private:
	// Emission
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;

	// Array of particle data
	Particle* particles;
	int maxParticles;

	// Lifetime tracking
	float lifetime; 
	int indexFirstDead;
	int indexFirstAlive;
	int livingParticleCount;

	// Emitter-level data (this is the same for all particles)
	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 emitterPosition;
	DirectX::XMFLOAT3 startVelocity;

	// Particle visual data (interpolated
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;

	// Particle randomization ranges
	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT2 rotationStartMinMax;
	DirectX::XMFLOAT2 rotationEndMinMax;

	// Sprite sheet options
	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;
	float spriteSheetSpeedScale;
	
	// Rendering
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
	SimpleVertexShader* vs;
	SimplePixelShader* ps;

	// Simulation methods
	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};

