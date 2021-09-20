#include "Material.h"



Material::Material(
	SimpleVertexShader* vs,
	SimplePixelShader* ps,
	DirectX::XMFLOAT4 color,
	float shininess,
	DirectX::XMFLOAT2 uvScale)
{
	this->vs = vs;
	this->ps = ps;
	this->color = color;
	this->shininess = shininess;
	this->uvScale = uvScale;
}


Material::~Material()
{
}

void Material::PrepareMaterial(Transform* transform, Camera* cam)
{
	// Turn shaders on
	vs->SetShader();
	ps->SetShader();

	// Set vertex shader data
	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
	vs->SetMatrix4x4("worldInverseTranspose", transform->GetWorldInverseTransposeMatrix());
	vs->SetMatrix4x4("view", cam->GetView());
	vs->SetMatrix4x4("projection", cam->GetProjection());
	vs->SetFloat2("uvScale", uvScale);
	vs->CopyAllBufferData();

	// Set pixel shader data
	ps->SetFloat4("Color", color); 
	ps->SetFloat("Shininess", shininess);
	ps->CopyBufferData("perMaterial");

	// Loop and set any other resources
	for (auto t : psTextureSRVs) { ps->SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto t : vsTextureSRVs) { vs->SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto s : psSamplers) { ps->SetSamplerState(s.first.c_str(), s.second); }
	for (auto s : vsSamplers) { vs->SetSamplerState(s.first.c_str(), s.second); }
}

void Material::SetPerMaterialDataAndResources(bool copyToGPUNow)
{
	// Set vertex shader per-material vars
	vs->SetFloat2("uvScale", uvScale);
	if (copyToGPUNow)
	{
		vs->CopyBufferData("perMaterial");
	}

	// Set pixel shader per-material vars
	ps->SetFloat4("Color", color);
	ps->SetFloat("Shininess", shininess);
	if (copyToGPUNow)
	{
		ps->CopyBufferData("perMaterial");
	}

	// Loop and set any other resources
	for (auto t : psTextureSRVs) { ps->SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto t : vsTextureSRVs) { vs->SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto s : psSamplers) { ps->SetSamplerState(s.first.c_str(), s.second); }
	for (auto s : vsSamplers) { vs->SetSamplerState(s.first.c_str(), s.second); }
}

void Material::AddPSTextureSRV(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
{
	psTextureSRVs.insert({ shaderName, srv });
}

void Material::AddVSTextureSRV(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
{
	vsTextureSRVs.insert({ shaderName, srv });
}

void Material::AddPSSampler(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	psSamplers.insert({ shaderName, sampler });
}

void Material::AddVSSampler(std::string shaderName, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	vsSamplers.insert({ shaderName, sampler });
}