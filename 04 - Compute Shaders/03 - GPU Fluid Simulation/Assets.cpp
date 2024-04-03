#include "Assets.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
// If using C++17, remove the "experimental" portion above and anywhere filesystem is used!

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>


// Singleton requirement
Assets* Assets::instance;

Assets::~Assets()
{
	// Delete all regular pointers
	for (auto& m : meshes) delete m.second;
	for (auto& p : pixelShaders) delete p.second;
	for (auto& v : vertexShaders) delete v.second;
	for (auto& c : computeShaders) delete c.second;
}


void Assets::Initialize(std::string rootAssetPath, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	this->device = device;
	this->context = context;
	this->rootAssetPath = rootAssetPath;
}


void Assets::LoadAllAssets()
{
	if (rootAssetPath.empty())
		return;
	
	// Recursively go through all directories starting at the root
	for (auto& item : std::experimental::filesystem::recursive_directory_iterator(GetFullPathTo(rootAssetPath)))
	{
		// Is this a regular file?
		if (item.status().type() == std::experimental::filesystem::file_type::regular)
		{
			std::string itemPath = item.path().string();

			// Determine the file type
			if (EndsWith(itemPath, ".obj") || EndsWith(itemPath, ".fbx"))
			{
				LoadMesh(itemPath);
			}
			else if (EndsWith(itemPath, ".jpg") || EndsWith(itemPath, ".png"))
			{
				LoadTexture(itemPath);
			}
			else if (EndsWith(itemPath, ".dds"))
			{
				LoadDDSTexture(itemPath);
			}
		}
	}

	// Search and load all shaders in the exe directory
	for (auto& item : std::experimental::filesystem::directory_iterator(GetFullPathTo(".")))
	{
		// Assume we're just using the filename for shaders due to being in the .exe path
		std::string itemPath = item.path().filename().string();

		// Is this a Compiled Shader Object?
		if (EndsWith(itemPath, ".cso"))
		{
			LoadUnknownShader(itemPath);
		}
	}
}



Mesh* Assets::GetMesh(std::string name)
{
	// Search and return mesh if found
	auto it = meshes.find(name);
	if (it != meshes.end())
		return it->second;

	// Unsuccessful
	return 0;
}

SimplePixelShader* Assets::GetPixelShader(std::string name)
{
	// Search and return shader if found
	auto it = pixelShaders.find(name);
	if (it != pixelShaders.end())
		return it->second;

	// Unsuccessful
	return 0;
}

SimpleVertexShader* Assets::GetVertexShader(std::string name)
{
	// Search and return shader if found
	auto it = vertexShaders.find(name);
	if (it != vertexShaders.end())
		return it->second;

	// Unsuccessful
	return 0;
}

SimpleComputeShader* Assets::GetComputeShader(std::string name)
{
	// Search and return shader if found
	auto it = computeShaders.find(name);
	if (it != computeShaders.end())
		return it->second;

	// Unsuccessful
	return 0;
}



Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Assets::GetTexture(std::string name)
{
	// Search and return texture if found
	auto it = textures.find(name);
	if (it != textures.end())
		return it->second;

	// Unsuccessful
	return 0;
}



void Assets::LoadMesh(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	printf("Loading mesh: ");
	printf(filename.c_str());
	printf("\n");


	// Load the mesh
	Mesh* m = new Mesh(path.c_str(), device, true);
	
	// Add to the dictionary
	meshes.insert({ filename, m });
}

void Assets::LoadTexture(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	printf("Loading texture: ");
	printf(filename.c_str());
	printf("\n");

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateWICTextureFromFile(device.Get(), context.Get(), ToWideString(path).c_str(), 0, srv.GetAddressOf());

	// Add to the dictionary
	textures.insert({ filename, srv });
}



void Assets::LoadDDSTexture(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	printf("Loading texture: ");
	printf(filename.c_str());
	printf("\n");

	// Load the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	DirectX::CreateDDSTextureFromFile(device.Get(), context.Get(), ToWideString(path).c_str(), 0, srv.GetAddressOf());

	// Add to the dictionary
	textures.insert({ filename, srv });
}


void Assets::LoadUnknownShader(std::string path)
{
	// Load the file into a blob
	ID3DBlob* shaderBlob;
	HRESULT hr = D3DReadFileToBlob(GetFullPathTo_Wide(ToWideString(path)).c_str(), &shaderBlob);
	if (hr != S_OK)
	{
		return;
	}

	// Set up shader reflection to get information about
	// this shader and its variables,  buffers, etc.
	ID3D11ShaderReflection* refl;
	D3DReflect(
		shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)&refl);

	// Get the description of the shader
	D3D11_SHADER_DESC shaderDesc;
	refl->GetDesc(&shaderDesc);

	// What kind of shader?
	switch (D3D11_SHVER_GET_TYPE(shaderDesc.Version))
	{
	case D3D11_SHVER_VERTEX_SHADER: LoadVertexShader(path); break;
	case D3D11_SHVER_PIXEL_SHADER: LoadPixelShader(path); break;
	case D3D11_SHVER_COMPUTE_SHADER: LoadComputeShader(path); break;
	}

	// Clean up
	refl->Release();
	shaderBlob->Release();
}


void Assets::LoadPixelShader(std::string path, bool useAssetPath)
{
	// Assuming filename and path are the same
	std::string filename = path;

	// Unless we need to check asset folder
	if (useAssetPath)
	{
		// Strip out everything before and including the asset root path
		size_t assetPathLength = rootAssetPath.size();
		size_t assetPathPosition = path.rfind(rootAssetPath);
		filename = path.substr(assetPathPosition + assetPathLength);
	}

	printf("Loading pixel shader: ");
	printf(filename.c_str());
	printf("\n");

	// Create the simple shader and add to dictionary
	SimplePixelShader* ps = new SimplePixelShader(device, context, GetFullPathTo_Wide(ToWideString(path)).c_str());
	pixelShaders.insert({ filename, ps });

}


void Assets::LoadVertexShader(std::string path, bool useAssetPath)
{
	// Assuming filename and path are the same
	std::string filename = path;

	// Unless we need to check asset folder
	if (useAssetPath)
	{
		// Strip out everything before and including the asset root path
		size_t assetPathLength = rootAssetPath.size();
		size_t assetPathPosition = path.rfind(rootAssetPath);
		filename = path.substr(assetPathPosition + assetPathLength);
	}

	printf("Loading vertex shader: ");
	printf(filename.c_str());
	printf("\n");

	// Create the simple shader and add to dictionary
	SimpleVertexShader* vs = new SimpleVertexShader(device, context, GetFullPathTo_Wide(ToWideString(path)).c_str());
	vertexShaders.insert({ filename, vs });
}


void Assets::LoadComputeShader(std::string path, bool useAssetPath)
{
	// Assuming filename and path are the same
	std::string filename = path;

	// Unless we need to check asset folder
	if (useAssetPath)
	{
		// Strip out everything before and including the asset root path
		size_t assetPathLength = rootAssetPath.size();
		size_t assetPathPosition = path.rfind(rootAssetPath);
		filename = path.substr(assetPathPosition + assetPathLength);
	}

	printf("Loading compute shader: ");
	printf(filename.c_str());
	printf("\n");

	// Create the simple shader and add to dictionary
	SimpleComputeShader* vs = new SimpleComputeShader(device, context, GetFullPathTo_Wide(ToWideString(path)).c_str());
	computeShaders.insert({ filename, vs });
}

// --------------------------------------------------------------------------
// Creates a solid color texture of the specified size and adds it to
// the asset manager using the specified name
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// color - color for each pixel of the texture
// --------------------------------------------------------------------------
void Assets::CreateSolidColorTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4 color)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return;

	// Create an array of the color
	DirectX::XMFLOAT4* pixels = new DirectX::XMFLOAT4[width * height];
	for (int i = 0; i < width * height; i++)
	{
		pixels[i] = color;
	}

	// Create the texture itself
	CreateTexture(textureName, width, height, pixels);
	

	// All done with pixel array
	delete[] pixels;
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
void Assets::CreateTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return;

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
}

// --------------------------------------------------------------------------
// Creates a texture of the specified size, using the specified colors as the
// texture's pixel colors and adds it to the asset manager using the specified name.
// The resulting texture will hold arbitrary float values instead of 0-1 values.
// The texture format will be DXGI_FORMAT_R32G32B32A32_FLOAT
//
// textureName - name to use in the asset manager
// width - width of the texture
// height - height of the texture
// pixels - color array for each pixel of the texture
// --------------------------------------------------------------------------
void Assets::CreateFloatTexture(std::string textureName, int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return;

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
}


// --------------------------------------------------------------------------
// Gets the actual path to this executable
//
// - As it turns out, the relative path for a program is different when 
//    running through VS and when running the .exe directly, which makes 
//    it a pain to properly load files external files (like textures)
//    - Running through VS: Current Dir is the *project folder*
//    - Running from .exe:  Current Dir is the .exe's folder
// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
//    Visual Studio "thing", and isn't obvious unless you know to look 
//    for it.  In fact, it could be fixed by changing a setting in VS, but
//    the option is stored in a user file (.suo), which is ignored by most
//    version control packages by default.  Meaning: the option must be
//    changed every on every PC.  Ugh.  So instead, here's a helper.
// --------------------------------------------------------------------------
std::string Assets::GetExePath()
{
	// Assume the path is just the "current directory" for now
	std::string path = ".\\";

	// Get the real, full path to this executable
	char currentDir[1024] = {};
	GetModuleFileName(0, currentDir, 1024);

	// Find the location of the last slash charaacter
	char* lastSlash = strrchr(currentDir, '\\');
	if (lastSlash)
	{
		// End the string at the last slash character, essentially
		// chopping off the exe's file name.  Remember, c-strings
		// are null-terminated, so putting a "zero" character in 
		// there simply denotes the end of the string.
		*lastSlash = 0;

		// Set the remainder as the path
		path = currentDir;
	}

	// Toss back whatever we've found
	return path;
}


// ---------------------------------------------------
//  Same as GetExePath(), except it returns a wide character
//  string, which most of the Windows API requires.
// ---------------------------------------------------
std::wstring Assets::GetExePath_Wide()
{
	// Grab the path as a standard string
	std::string path = GetExePath();

	// Convert to a wide string
	wchar_t widePath[1024] = {};
	mbstowcs_s(0, widePath, path.c_str(), 1024);

	// Create a wstring for it and return
	return std::wstring(widePath);
}


// ----------------------------------------------------
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::string Assets::GetFullPathTo(std::string relativeFilePath)
{
	return GetExePath() + "\\" + relativeFilePath;
}



// ----------------------------------------------------
//  Same as GetFullPathTo, but with wide char strings.
// 
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::wstring Assets::GetFullPathTo_Wide(std::wstring relativeFilePath)
{
	return GetExePath_Wide() + L"\\" + relativeFilePath;
}

bool Assets::EndsWith(std::string str, std::string ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}

std::wstring Assets::ToWideString(std::string str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(str);
}
