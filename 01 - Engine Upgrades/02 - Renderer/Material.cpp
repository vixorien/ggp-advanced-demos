#include "Material.h"

Material::Material(
	std::shared_ptr<SimplePixelShader> ps, 
	std::shared_ptr<SimpleVertexShader> vs, 
	DirectX::XMFLOAT3 tint, 
	DirectX::XMFLOAT2 uvScale,
	DirectX::XMFLOAT2 uvOffset) 
	:
	ps(ps),
	vs(vs),
	colorTint(tint),
	uvScale(uvScale),
	uvOffset(uvOffset)
{

}

// Getters
std::shared_ptr<SimplePixelShader> Material::GetPixelShader() { return ps; }
std::shared_ptr<SimpleVertexShader> Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT2 Material::GetUVScale() { return uvScale; }
DirectX::XMFLOAT2 Material::GetUVOffset() { return uvOffset; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Material::GetTextureSRV(std::string name)
{
	// Search for the key
	auto it = textureSRVs.find(name);

	// Not found, return null
	if (it == textureSRVs.end())
		return 0;

	// Return the texture ComPtr
	return it->second;
}

Microsoft::WRL::ComPtr<ID3D11SamplerState> Material::GetSampler(std::string name)
{
	// Search for the key
	auto it = samplers.find(name);

	// Not found, return null
	if (it == samplers.end())
		return 0;

	// Return the sampler ComPtr
	return it->second;
}

// Setters
void Material::SetPixelShader(std::shared_ptr<SimplePixelShader> ps) { this->ps = ps; }
void Material::SetVertexShader(std::shared_ptr<SimpleVertexShader> vs) { this->vs = vs; }
void Material::SetUVScale(DirectX::XMFLOAT2 scale) { uvScale = scale; }
void Material::SetUVOffset(DirectX::XMFLOAT2 offset) { uvOffset = offset; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }


void Material::AddTextureSRV(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
{
	textureSRVs.insert({ name, srv });
}

void Material::AddSampler(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	samplers.insert({ name, sampler });
}

void Material::RemoveTextureSRV(std::string name)
{
	textureSRVs.erase(name);
}

void Material::RemoveSampler(std::string name)
{
	samplers.erase(name);
}


void Material::PrepareMaterial(Transform* transform, std::shared_ptr<Camera> camera)
{
	// Turn on these shaders
	vs->SetShader();
	ps->SetShader();

	// Send data to the vertex shader
	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
	vs->SetMatrix4x4("worldInverseTranspose", transform->GetWorldInverseTransposeMatrix());
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->CopyAllBufferData();

	// Send data to the pixel shader
	ps->SetFloat3("colorTint", colorTint);
	ps->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
	ps->SetFloat2("uvScale", uvScale);
	ps->SetFloat2("uvOffset", uvOffset);
	ps->CopyAllBufferData();

	// Loop and set any other resources
	for (auto& t : textureSRVs) { ps->SetShaderResourceView(t.first.c_str(), t.second.Get()); }
	for (auto& s : samplers) { ps->SetSamplerState(s.first.c_str(), s.second.Get()); }
}
