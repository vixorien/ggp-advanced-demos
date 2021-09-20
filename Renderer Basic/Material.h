#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "SimpleShader.h"
#include "Camera.h"
#include "Lights.h"

class Material
{
public:
	Material(
		SimpleVertexShader* vs, 
		SimplePixelShader* ps, 
		DirectX::XMFLOAT4 color, 
		float shininess, 
		DirectX::XMFLOAT2 uvScale, 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo, 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normals, 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughness, 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal, 
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);
	~Material();

	void PrepareMaterial(Transform* transform, Camera* cam);

	SimpleVertexShader* GetVS() { return vs; }
	SimplePixelShader* GetPS() { return ps; }

	void SetVS(SimpleVertexShader* vs) { this->vs = vs; }
	void SetPS(SimplePixelShader* ps) { this->ps = ps; }

private:
	SimpleVertexShader* vs;
	SimplePixelShader* ps;

	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT4 color;
	float shininess;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughnessSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalSRV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
};

