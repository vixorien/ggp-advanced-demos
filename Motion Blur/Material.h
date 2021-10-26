#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <unordered_map>

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
		DirectX::XMFLOAT2 uvScale);
	~Material();

	void PrepareMaterial(Transform* transform, Camera* cam);
	void SetPerMaterialDataAndResources(bool copyToGPUNow = true);

	SimpleVertexShader* GetVS() { return vs; }
	SimplePixelShader* GetPS() { return ps; }

	void SetVS(SimpleVertexShader* vs) { this->vs = vs; }
	void SetPS(SimplePixelShader* ps) { this->ps = ps; }

	void AddPSTextureSRV(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
	void AddVSTextureSRV(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
	void AddPSSampler(std::string samplerName, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);
	void AddVSSampler(std::string samplerName, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);

private:
	SimpleVertexShader* vs;
	SimplePixelShader* ps;

	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT4 color;
	float shininess;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> psTextureSRVs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> vsTextureSRVs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11SamplerState>> psSamplers;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11SamplerState>> vsSamplers;
};

