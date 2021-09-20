#include "Material.h"



Material::Material(
	SimpleVertexShader* vs,
	SimplePixelShader* ps,
	DirectX::XMFLOAT4 color,
	float shininess,
	DirectX::XMFLOAT2 uvScale,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normals,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughness,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	this->vs = vs;
	this->ps = ps;
	this->color = color;
	this->shininess = shininess;
	this->albedoSRV = albedo;
	this->normalSRV = normals;
	this->roughnessSRV = roughness;
	this->metalSRV = metal;
	this->sampler = sampler;
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

	// Set SRVs
	ps->SetShaderResourceView("AlbedoTexture", albedoSRV);
	ps->SetShaderResourceView("NormalTexture", normalSRV);
	ps->SetShaderResourceView("RoughnessTexture", roughnessSRV);
	ps->SetShaderResourceView("MetalTexture", metalSRV);

	// Set sampler
	ps->SetSamplerState("BasicSampler", sampler);
}
