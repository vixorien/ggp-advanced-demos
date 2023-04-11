#include "Assets.h"
#include "Helpers.h"

#include <fstream>
#include "../../Common/json/json.hpp"
using json = nlohmann::json;

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
// If using C++17, remove the L"experimental" portion above and anywhere filesystem is used!

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>


// Singleton requirement
Assets* Assets::instance;


// --------------------------------------------------------------------------
// Cleans up the asset manager and deletes any resources that are
// not stored with smart pointers.
// --------------------------------------------------------------------------
Assets::~Assets()
{
}



// --------------------------------------------------------------------------
// Initializes the asset manager with the D3D objects it may need, as 
// well as the root asset path to check for assets.  Note that shaders
// are loaded from the executables path by default.
// --------------------------------------------------------------------------
void Assets::Initialize(
	std::wstring rootAssetPath, 
	std::wstring rootShaderPath, 
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
	bool printLoadingProgress, 
	bool allowOnDemandLoading)
{
	this->device = device;
	this->context = context;
	this->rootAssetPath = rootAssetPath;
	this->rootShaderPath = rootShaderPath;
	this->printLoadingProgress = printLoadingProgress;
	this->allowOnDemandLoading = allowOnDemandLoading;

	// Replace all L"\\" with L"/" to ease lookup later
	std::replace(this->rootAssetPath.begin(), this->rootAssetPath.end(), '\\', '/');
	std::replace(this->rootShaderPath.begin(), this->rootShaderPath.end(), '\\', '/');

	// Ensure the root paths end with slashes
	if (!EndsWith(rootAssetPath, L"/")) rootAssetPath += L"/";
	if (!EndsWith(rootShaderPath, L"/")) rootShaderPath += L"/";
}


// --------------------------------------------------------------------------
// Recursively checks all files starting in the root asset path, determines
// if they are files that can be loaded and loads each one.
// 
// Currently, only the following file types are supported:
//  - Textures: .jpg, .png, .dds
//  - Meshes: .obj
//  - Sprite Font: .spritefont
//  - Shaders: .cso (these are loaded from the executable's path!)
// --------------------------------------------------------------------------
void Assets::LoadAllAssets()
{
	if (rootAssetPath.empty()) return;
	if (rootShaderPath.empty()) return;
	
	// These files need to be loaded after all basic assets
	std::vector<std::wstring> materialPaths;

	// Recursively go through all directories starting at the root
	for (auto& item : std::experimental::filesystem::recursive_directory_iterator(FixPath(rootAssetPath)))
	{
		// Is this a regular file?
		if (item.status().type() == std::experimental::filesystem::file_type::regular)
		{
			std::wstring itemPath = item.path().wstring();

			// Replace all L"\\" with L"/" to ease lookup later
			std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

			// Determine the file type
			if (EndsWith(itemPath, L".obj"))
			{
				LoadMesh(itemPath);
			}
			else if (EndsWith(itemPath, L".jpg") || EndsWith(itemPath, L".png"))
			{
				LoadTexture(itemPath);
			}
			else if (EndsWith(itemPath, L".dds"))
			{
				LoadDDSTexture(itemPath);
			}
			else if (EndsWith(itemPath, L".spritefont"))
			{
				LoadSpriteFont(itemPath);
			}
			else if (EndsWith(itemPath, L".sampler"))
			{
				LoadSampler(itemPath);
			}
			else if (EndsWith(itemPath, L".material"))
			{
				materialPaths.push_back(itemPath);
			}
		}
	}

	// Search and load all shaders in the shader path
	for (auto& item : std::experimental::filesystem::directory_iterator(FixPath(rootShaderPath)))
	{
		std::wstring itemPath = item.path().wstring();

		// Replace all L"\\" with L"/" to ease lookup later
		std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

		// Is this a Compiled Shader Object?
		if (EndsWith(itemPath, L".cso"))
		{
			LoadUnknownShader(itemPath);
		}
	}

	// Load all materials
	for (auto& mPath : materialPaths)
	{
		LoadMaterial(mPath);
	}
	
}



// --------------------------------------------------------------------------
// Gets the specified mesh if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the mesh if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
//
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Models/cube"
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::GetMesh(std::wstring name)
{
	// Search and return mesh if found
	auto it = meshes.find(name);
	if (it != meshes.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".obj");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadMesh(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified material if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the material (a JSON file)
// if it doesn't exist in the asset manager.  Otherwise, this method returns null.
//
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Materials/cobblestone2xPBR"
// --------------------------------------------------------------------------
std::shared_ptr<Material> Assets::GetMaterial(std::wstring name)
{
	// Search and return mesh if found
	auto it = materials.find(name);
	if (it != materials.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".material");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadMaterial(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified sprite font if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the sprite font if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Fonts/Arial12"
// --------------------------------------------------------------------------
std::shared_ptr<DirectX::SpriteFont> Assets::GetSpriteFont(std::wstring name)
{
	// Search and return mesh if found
	auto it = spriteFonts.find(name);
	if (it != spriteFonts.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".spritefont");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadSpriteFont(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified sampler if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the sampler if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Samplers/Anisotropic16Wrap"
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11SamplerState> Assets::GetSampler(std::wstring name)
{
	// Search and return mesh if found
	auto it = samplers.find(name);
	if (it != samplers.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".sampler");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadSampler(filePath);
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified texture if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the texture if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Textures/PBR/cobblestone_albedo"
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::GetTexture(std::wstring name)
{
	// Search and return texture if found
	auto it = textures.find(name);
	if (it != textures.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::wstring filePathJPG = FixPath(rootAssetPath + name + L".jpg");
		if (std::experimental::filesystem::exists(filePathJPG)) { return LoadTexture(filePathJPG); }

		// Is it a PNG?
		std::wstring filePathPNG = FixPath(rootAssetPath + name + L".png");
		if (std::experimental::filesystem::exists(filePathPNG)) { return LoadTexture(filePathPNG); }

		// Is it a DDS?
		std::wstring filePathDDS = FixPath(rootAssetPath + name + L".dds");
		if (std::experimental::filesystem::exists(filePathDDS)) { return LoadDDSTexture(filePathDDS); }
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified pixel shader if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the pixel shader if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do NOT include the path, unless it is outside the executable's folder
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"SkyPS"
// --------------------------------------------------------------------------
std::shared_ptr<SimplePixelShader> Assets::GetPixelShader(std::wstring name)
{
	// Search and return shader if found
	auto it = pixelShaders.find(name);
	if (it != pixelShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootShaderPath + name + L".cso");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			std::shared_ptr<SimplePixelShader> ps = LoadPixelShader(filePath);
			if (ps) { return ps; }
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Gets the specified vertex shader if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the vertex shader if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
// 
// Notes on file names:
//  - Do NOT include the path, unless it is outside the executable's folder
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"SkyVS"
// --------------------------------------------------------------------------
std::shared_ptr<SimpleVertexShader> Assets::GetVertexShader(std::wstring name)
{
	// Search and return shader if found
	auto it = vertexShaders.find(name);
	if (it != vertexShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootShaderPath + name + L".cso");
		if (std::experimental::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			std::shared_ptr<SimpleVertexShader> vs = LoadVertexShader(filePath);
			if (vs) { return vs; }
		}
	}

	// Unsuccessful
	return 0;
}


// --------------------------------------------------------------------------
// Adds an existing mesh to the asset manager.
// 
// Note: The asset manager will clean up (delete) this mesh at shut down.
// Make sure you're not also deleting it yourself later.
// --------------------------------------------------------------------------
void Assets::AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh)
{
	meshes.insert({ name, mesh });
}


// --------------------------------------------------------------------------
// Adds an existing sprite font to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddMaterial(std::wstring name, std::shared_ptr<Material> material)
{
	materials.insert({name, material});
}


// --------------------------------------------------------------------------
// Adds an existing sprite font to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddSpriteFont(std::wstring name, std::shared_ptr<DirectX::SpriteFont> font)
{
	spriteFonts.insert({ name, font });
}


// --------------------------------------------------------------------------
// Adds an existing pixel shader to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddPixelShader(std::wstring name, std::shared_ptr<SimplePixelShader> ps)
{
	pixelShaders.insert({ name, ps });
}


// --------------------------------------------------------------------------
// Adds an existing vertex shader to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddVertexShader(std::wstring name, std::shared_ptr<SimpleVertexShader> vs)
{
	vertexShaders.insert({ name, vs });
}

// --------------------------------------------------------------------------
// Adds an existing sampler to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddSampler(std::wstring name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	samplers.insert({ name, sampler });
}


// --------------------------------------------------------------------------
// Adds an existing texture to the asset manager.
// --------------------------------------------------------------------------
void Assets::AddTexture(std::wstring name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture)
{
	textures.insert({ name, texture });
}


// --------------------------------------------------------------------------
// Getters for asset counts
// --------------------------------------------------------------------------
unsigned int Assets::GetMeshCount() { return (unsigned int)meshes.size(); }
unsigned int Assets::GetMaterialCount() { return (unsigned int)materials.size(); }
unsigned int Assets::GetSpriteFontCount() { return (unsigned int)spriteFonts.size(); }
unsigned int Assets::GetPixelShaderCount() { return (unsigned int)pixelShaders.size(); }
unsigned int Assets::GetVertexShaderCount() { return (unsigned int)vertexShaders.size(); }
unsigned int Assets::GetSamplerCount() { return (unsigned int)samplers.size(); }
unsigned int Assets::GetTextureCount() { return (unsigned int)textures.size(); }



// --------------------------------------------------------------------------
// Private helper for loading a mesh from an .obj file
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::LoadMesh(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading mesh: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<Mesh> m = std::make_shared<Mesh>(path.c_str(), device);
	
	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	meshes.insert({ filename, m });
	return m;
}


// --------------------------------------------------------------------------
// Private helper for loading a material from a .json file
// --------------------------------------------------------------------------
std::shared_ptr<Material> Assets::LoadMaterial(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading material: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);
	
	// Verify required members (shaders for now)
	if (d.is_discarded() ||
		!d.contains("shaders") ||
		!d["shaders"].contains("pixel") ||
		!d["shaders"].contains("vertex"))
	{
		std::shared_ptr<SimplePixelShader> psInvalid = 0;
		std::shared_ptr<SimpleVertexShader> vsInvalid = 0;
		std::shared_ptr<Material> matInvalid = std::make_shared<Material>(psInvalid, vsInvalid); // TODO: Default shaders?
		AddMaterial(filename, matInvalid);
		return matInvalid;
	}

	// Check to see if the requested shaders exist
	std::wstring psName = NarrowToWide(d["shaders"]["pixel"].get<std::string>());
	std::wstring vsName = NarrowToWide(d["shaders"]["vertex"].get<std::string>());

	std::shared_ptr<SimplePixelShader> ps = GetPixelShader(psName);
	std::shared_ptr<SimpleVertexShader> vs = GetVertexShader(vsName);

	// We have enough to make the material
	std::shared_ptr<Material> mat = std::make_shared<Material>(ps, vs);
	
	// Check for 3-component tint
	if (d.contains("tint") && d["tint"].size() == 3)
	{
		DirectX::XMFLOAT3 tint(0, 0, 0);
		tint.x = d["tint"][0].get<float>();
		tint.y = d["tint"][1].get<float>();
		tint.z = d["tint"][2].get<float>();
		mat->SetColorTint(tint);
	}

	// 2-component uvScale
	if (d.contains("uvScale") && d["uvScale"].size() == 2)
	{
		DirectX::XMFLOAT2 uvScale(0, 0);
		uvScale.x = d["uvScale"][0].get<float>();
		uvScale.y = d["uvScale"][1].get<float>();
		mat->SetUVScale(uvScale);
	}

	// 2-component uvOffset
	if (d.contains("uvOffset") && d["uvOffset"].size() == 2)
	{
		DirectX::XMFLOAT2 uvOffset(0, 0);
		uvOffset.x = d["uvOffset"][0].get<float>();
		uvOffset.y = d["uvOffset"][1].get<float>();
		mat->SetUVOffset(uvOffset);
	}

	// Check for samplers
	if (d.contains("samplers"))
	{
		for (unsigned int s = 0; s < d["samplers"].size(); s++)
		{
			// Do we know about this sampler?
			std::wstring samplerName = NarrowToWide(d["samplers"][s]["name"].get<std::string>());
			Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler = GetSampler(samplerName);
			if (sampler)
			{
				mat->AddSampler(d["samplers"][s]["shaderName"].get<std::string>(), sampler);
			}
		}
	}

	// Check for textures
	if (d.contains("textures"))
	{
		for (unsigned int t = 0; t < d["textures"].size(); t++)
		{
			// Do we know about this texture?
			std::wstring textureName = NarrowToWide(d["textures"][t]["name"].get<std::string>());
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture = GetTexture(textureName);
			if (texture)
			{
				mat->AddTextureSRV(d["textures"][t]["shaderName"].get<std::string>(), texture);
			}
		}
	}

	// Add the material to our list and return it
	materials.insert({ filename, mat });
	return mat;
}


// --------------------------------------------------------------------------
// Private helper for loading a .spritefont file
// --------------------------------------------------------------------------
std::shared_ptr<DirectX::SpriteFont> Assets::LoadSpriteFont(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading sprite font: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<DirectX::SpriteFont> font = std::make_shared<DirectX::SpriteFont>(device.Get(), path.c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	spriteFonts.insert({ filename, font });
	return font;
}



// --------------------------------------------------------------------------
// Private helper for loading a .sampler file
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11SamplerState> Assets::LoadSampler(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading sampler: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	if (d.is_discarded())
	{
		// Set a null sampler
		samplers.insert({ filename, 0 });
		return 0;
	}

	// Set up the description with the defaults, to be overridden
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.MipLODBias = 0.0f;
	sampDesc.MinLOD = -D3D11_FLOAT32_MAX;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.BorderColor[0] = 1.0f;
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;

	if (d.contains("filter") && d["filter"].is_string())
	{
		std::string filter = d["filter"].get<std::string>();
		if (filter == "point") sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (filter == "linear") sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		else if (filter == "anisotropic") sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		else if (filter == "comparisonPoint") sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else if (filter == "comparisonLinear") sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		else if (filter == "comparisonAnisotropic") sampDesc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
	}

	if (d.contains("addressMode") && d["addressMode"].is_string())
	{
		std::string addr = d["addressMode"].get<std::string>();

		D3D11_TEXTURE_ADDRESS_MODE mode = D3D11_TEXTURE_ADDRESS_WRAP;
		if (addr == "wrap") mode = D3D11_TEXTURE_ADDRESS_WRAP;
		else if (addr == "clamp") mode = D3D11_TEXTURE_ADDRESS_CLAMP;
		else if (addr == "mirror") mode = D3D11_TEXTURE_ADDRESS_MIRROR;
		else if (addr == "mirrorOnce") mode = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
		else if (addr == "border") mode = D3D11_TEXTURE_ADDRESS_BORDER;

		sampDesc.AddressU = mode;
		sampDesc.AddressV = mode;
		sampDesc.AddressW = mode;
	}

	if (d.contains("maxAnisotropy") && d["maxAnisotropy"].is_number_integer())
	{
		sampDesc.MaxAnisotropy = d["maxAnisotropy"].get<int>();
	}

	if (d.contains("borderColor") && d["borderColor"].size() == 4)
	{
		sampDesc.BorderColor[0] = d["borderColor"][0].get<float>();
		sampDesc.BorderColor[1] = d["borderColor"][1].get<float>();
		sampDesc.BorderColor[2] = d["borderColor"][2].get<float>();
		sampDesc.BorderColor[3] = d["borderColor"][3].get<float>();
	}

	if (d.contains("comparison") && d["comparison"].is_string())
	{
		std::string comp = d["comparison"].get<std::string>();
		if (comp == "never") sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		else if (comp == "less") sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
		else if (comp == "equal") sampDesc.ComparisonFunc = D3D11_COMPARISON_EQUAL;
		else if (comp == "lessEqual") sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		else if (comp == "greater") sampDesc.ComparisonFunc = D3D11_COMPARISON_GREATER;
		else if (comp == "notEqual") sampDesc.ComparisonFunc = D3D11_COMPARISON_NOT_EQUAL;
		else if (comp == "greaterEqual") sampDesc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;
		else if (comp == "always") sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	}

	// Create the state and save
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	samplers.insert({ filename, sampler });
	return sampler;
}



// --------------------------------------------------------------------------
// Private helper for loading a standard BMP/PNG/JPG/TIF texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::LoadTexture(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateWICTextureFromFile(device.Get(), context.Get(), path.c_str(), 0, srv.GetAddressOf());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}



// --------------------------------------------------------------------------
// Private helper for loading a DDS texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::LoadDDSTexture(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateDDSTextureFromFile(device.Get(), context.Get(), path.c_str(), 0, srv.GetAddressOf());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}


// --------------------------------------------------------------------------
// Private helper for loading a compiled shader object (.cso) of unknown type.
// Currently, this supports vertex and pixel shaders
// --------------------------------------------------------------------------
void Assets::LoadUnknownShader(std::wstring path)
{
	// Load the file into a blob
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
	if (D3DReadFileToBlob(path.c_str(), shaderBlob.GetAddressOf()) != S_OK)
		return;

	// Set up shader reflection to get information about
	// this shader and its variables,  buffers, etc.
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> refl;
	D3DReflect(
		shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)refl.GetAddressOf());

	// Get the description of the shader
	D3D11_SHADER_DESC shaderDesc;
	refl->GetDesc(&shaderDesc);

	// What kind of shader?
	switch (D3D11_SHVER_GET_TYPE(shaderDesc.Version))
	{
	case D3D11_SHVER_VERTEX_SHADER: LoadVertexShader(path); break;
	case D3D11_SHVER_PIXEL_SHADER: LoadPixelShader(path); break;
	}
}



// --------------------------------------------------------------------------
// Private helper for loading a pixel shader from a .cso file
// --------------------------------------------------------------------------
std::shared_ptr<SimplePixelShader> Assets::LoadPixelShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootShaderPath.size();
	size_t shaderPathPosition = path.rfind(rootShaderPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading pixel shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	std::shared_ptr<SimplePixelShader> ps = std::make_shared<SimplePixelShader>(device, context, path.c_str());
	if (!ps->IsShaderValid()) { return 0; }

	// Success
	pixelShaders.insert({ filename, ps });
	return ps;

}


// --------------------------------------------------------------------------
// Private helper for loading a vertex shader from a .cso file
// --------------------------------------------------------------------------
std::shared_ptr<SimpleVertexShader> Assets::LoadVertexShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootShaderPath.size();
	size_t shaderPathPosition = path.rfind(rootShaderPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading vertex shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	std::shared_ptr<SimpleVertexShader> vs = std::make_shared<SimpleVertexShader>(device, context, path.c_str());
	if (!vs->IsShaderValid()) { return 0; }

	// Success
	vertexShaders.insert({ filename, vs });
	return vs;
}



// --------------------------------------------------------------------------
// Creates a solid color texture of the specified size and adds it to
// the asset manager using the specified name.  
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// color - color for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateSolidColorTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4 color)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Create an array of the color
	DirectX::XMFLOAT4* pixels = new DirectX::XMFLOAT4[width * height];
	for (int i = 0; i < width * height; i++)
	{
		pixels[i] = color;
	}

	// Create the texture itself
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = CreateTexture(textureName, width, height, pixels);
	
	// All done with pixel array
	delete[] pixels;

	// Return the SRV in the event it is immediately needed
	return srv;
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Convert to ints
	unsigned char* intPixels = new unsigned char[width * height * 4];
	for (int i = 0; i < width * height * 4;)
	{
		int pixelIndex = i / 4;
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].x * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].y * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].z * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].w * 255);
	}

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = intPixels;
	data.SysMemPitch = sizeof(unsigned char) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = td.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

	// Add to the asset manager
	textures.insert({ textureName, srv });

	// All done with these values
	delete[] intPixels;

	// Return the SRV in the event it is immediately needed
	return srv;
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name.
// The resulting texture will hold arbitrary float values instead of 0-1 values.
// The texture format will be DXGI_FORMAT_R32G32B32A32_FLOAT.
// 
// Also returns the resulting texture if it is valid, otherwise returns 0.
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::CreateFloatTexture(std::wstring textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = sizeof(float) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = td.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

	// Add to the asset manager
	textures.insert({ textureName, srv });

	// Return the SRV in the event it is immediately needed
	return srv;
}


// ----------------------------------------------------
// Determines if the given string ends with the given ending
// ----------------------------------------------------
bool Assets::EndsWith(std::wstring str, std::wstring ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}


// ----------------------------------------------------
// Remove the file extension by searching for the last 
// period character and removing everything afterwards
// ----------------------------------------------------
std::wstring Assets::RemoveFileExtension(std::wstring str)
{
	size_t found = str.find_last_of('.');
	return str.substr(0, found);
}
