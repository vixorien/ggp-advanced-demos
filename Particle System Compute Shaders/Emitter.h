#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

#include "SimpleShader.h"
#include "Camera.h"

struct Particle
{
	DirectX::XMFLOAT4 Color;
	float Age;
	DirectX::XMFLOAT3 Position;
	float Size;
	DirectX::XMFLOAT3 Velocity;
	float Alive;
	DirectX::XMFLOAT3 padding;
};

struct ParticleSort
{
	unsigned int index;
	float distanceSq;
};

class Emitter
{
public:
	Emitter(
		unsigned int maxParticles,
		float emissionRate, // Particles per second
		float lifetime,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		std::shared_ptr<SimpleComputeShader> deadListInitCS,
		std::shared_ptr<SimpleComputeShader> emitCS,
		std::shared_ptr<SimpleComputeShader> updateCS,
		std::shared_ptr<SimpleComputeShader> copyDrawCountCS,
		std::shared_ptr<SimpleVertexShader> particleVS,
		std::shared_ptr<SimplePixelShader> particlePS);
	~Emitter();

	void Update(float dt, float tt);
	void Draw(std::shared_ptr<Camera> camera, bool additive);

private:

	// Emitter settings
	unsigned int maxParticles;
	float lifetime;
	float emissionRate;
	float timeBetweenEmit;
	float emitTimeCounter;

	// Drawing
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11BlendState> additiveBlend;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthWriteOff;

	// DirectX stuff
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	std::shared_ptr<SimpleComputeShader> emitCS;
	std::shared_ptr<SimpleComputeShader> updateCS;
	std::shared_ptr<SimpleComputeShader> copyDrawCountCS;
	std::shared_ptr<SimpleVertexShader> particleVS;
	std::shared_ptr<SimplePixelShader> particlePS;

	// Indirect draw buffer
	Microsoft::WRL::ComPtr<ID3D11Buffer> drawArgsBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> deadListCounterBuffer;

	// Particle views
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particlePoolUAV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particlePoolSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDeadUAV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDrawUAV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDrawSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> drawArgsUAV;
};

