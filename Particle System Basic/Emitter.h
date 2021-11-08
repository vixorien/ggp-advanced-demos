#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>

#include "SimpleShader.h"
#include "Camera.h"

// We'll be mimicking this in HLSL
// so we need to care about alignment
struct Particle
{
	// 4 floats
	float SpawnTime;
	DirectX::XMFLOAT3 StartPosition;
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
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture
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
	void SpawnParticle(float currentTime);
};

