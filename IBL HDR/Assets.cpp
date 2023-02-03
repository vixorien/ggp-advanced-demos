#include "Assets.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
// If using C++17, remove the "experimental" portion above and anywhere filesystem is used!

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <fstream>

// Singleton requirement
Assets* Assets::instance;

Assets::~Assets()
{
	// Delete all regular pointers
	for (auto& m : meshes) delete m.second;
	for (auto& p : pixelShaders) delete p.second;
	for (auto& v : vertexShaders) delete v.second;
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
			else if (EndsWith(itemPath, ".hdr"))
			{
				LoadHDRTexture(itemPath);
			}
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


// Loads an HDR texture, converts to a cube map and generates mip maps for it.
// The mips are necessary to smooth out IBL map creation.
// 
// Note that this will take up a massive amount of space due to the texture
// format and mips.  Might be worth re-creating a mip-less version or 
// pre-tone-mapped version for the skybox and dumping this asset afterwards?
void Assets::LoadHDRTexture(std::string path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::string filename = path.substr(assetPathPosition + assetPathLength);

	printf("Loading texture: ");
	printf(filename.c_str());
	printf("\n");

	const char* HeaderSignature = "#?RADIANCE";
	const char* HeaderFormat = "FORMAT=32-bit_rle_rgbe";
	const unsigned int HeaderSigSize = 10;
	const unsigned int HeaderFormatSize = 22;

	// Basic read buffer
	char buffer[1024] = { 0 };

	bool invX = false;
	bool invY = false;

	// HEADER -----------------------------------

	// Open the file
	std::fstream file(path, std::ios_base::in | std::ios_base::binary);

	// Read the signature
	file.read(buffer, HeaderSigSize);
	if (strcmp(HeaderSignature, buffer) != 0)
		return;

	// Skip comment until we find FORMAT
	do {
		file.getline(buffer, sizeof(buffer));
	} while (!file.eof() && strncmp(buffer, "FORMAT", 6));

	// Did we hit the end of the file already?
	if (file.eof())
		return;

	// Invalid format!
	if (strcmp(buffer, HeaderFormat) != 0)
		return;

	// Check for Y inversion
	int x = 0;
	do {
		x++;
		file.getline(buffer, sizeof(buffer));
	} while (!file.eof() && (
		strncmp(buffer, "-Y", 2) &&
		strncmp(buffer, "+Y", 2))
		);

	// End of file while looking?
	if (file.eof())
		return;

	// Inverted?
	if (strncmp(buffer, "-Y", 2) == 0)
		invY = true;

	// Loop through buffer until X
	int counter = 0;
	while ((counter < sizeof(buffer)) && buffer[counter] != 'X')
		counter++;

	// No X?
	if (counter == sizeof(buffer))
		return;

	// Flipped X?
	if (buffer[counter - 1] == '-')
		invX = true;

	// Grab dimensions from current buffer line
	unsigned int width;
	unsigned int height;
	sscanf_s(buffer, "%*s %u %*s %u", &height, &width);

	// Got real dimensions?
	if (width == 0 || height == 0)
		return;

	// ACTUAL DATA ------------------------------

	unsigned int dataSize = width * height * 4;
	unsigned char* data = new unsigned char[dataSize];
	memset(data, 0, dataSize); // For testing

	// Scanline encoding
	char enc[4] = { 0 };

	// Loop through the scanlines, one at a time
	for (unsigned int y = 0; y < height; y++)
	{
		// Inverted Y doesn't seem to matter?
		int start = /*invY ? ((*height) - y - 1) * (*width) :*/ y * width;
		int step = /*invX ? -1 :*/ 1;

		// Check the encoding info for this line
		file.read(enc, 4);
		if (file.eof())
			break;

		// Which RLE scheme?
		if (enc[0] == 2 && enc[1] == 2 && enc[2] >= 0)
		{
			// NEW RLE SCHEME -------------------

			// Each component is RLE'd separately
			for (int c = 0; c < 4; c++)
			{
				int pos = start;
				unsigned int x = 0;

				// Loop through the pixels/components
				while (x < width)
				{
					char temp;
					file.read(&temp, 1);
					unsigned char num = reinterpret_cast<unsigned char&>(temp);

					// Check if this is a run
					if (num <= 128)
					{
						// No run, read the data
						for (int i = 0; i < num; i++)
						{
							char temp;
							file.read(&temp, 1);
							data[c + pos * 4] = reinterpret_cast<unsigned char&>(temp);

							pos += step;
						}
					}
					else
					{
						// Run!  Get the value and set everything
						char temp;
						file.read(&temp, 1);
						unsigned char value = reinterpret_cast<unsigned char&>(temp);

						num -= 128;
						for (int i = 0; i < num; i++)
						{
							data[c + pos * 4] = value;
							pos += step;
						}
					}

					// Move to the next section
					x += num;
				}
			}
		}
		else
		{
			// OLD RLE SCHEME -------------------

			int pos = start;

			// Loop through scanline
			for (unsigned int x = 0; x < width; x++)
			{
				if (x > 0)
				{
					file.read(enc, 4); // TODO: check for eof?
				}

				// Check for RLE header
				if (enc[0] == 1 && enc[1] == 1 && enc[2] == 1)
				{
					// RLE
					int num = ((int)enc[3]) & 0xFF;
					unsigned char r = data[(pos - step) * 4 + 0];
					unsigned char g = data[(pos - step) * 4 + 1];
					unsigned char b = data[(pos - step) * 4 + 2];
					unsigned char e = data[(pos - step) * 4 + 3];

					// Loop and set
					for (int i = 0; i < num; i++)
					{
						data[pos * 4 + 0] = r;
						data[pos * 4 + 1] = g;
						data[pos * 4 + 2] = b;
						data[pos * 4 + 3] = e;
						pos += step;
					}

					x += num - 1;
				}
				else
				{
					// No RLE, just read data
					data[pos * 4 + 0] = enc[0];
					data[pos * 4 + 1] = enc[1];
					data[pos * 4 + 2] = enc[2];
					data[pos * 4 + 3] = enc[3];
					pos += step;
				}

			}
		}
	}

	// Done with file
	file.close();


	// Convert data to final IEEE floats
	// Based on "Real Pixels" by Greg Ward in Graphics Gems II
	float* pixels = new float[dataSize];
	for (unsigned int i = 0; i < width * height; i++)
	{
		unsigned char exponent = data[i * 4 + 3];
		if (exponent == 0)
		{
			pixels[i * 4 + 0] = 0.0f;
			pixels[i * 4 + 1] = 0.0f;
			pixels[i * 4 + 2] = 0.0f;
			pixels[i * 4 + 3] = 1.0f;
		}
		else
		{
			float v = ldexp(1.0f / 256.0f, (int)(exponent - 128));
			pixels[i * 4 + 0] = (data[i * 4 + 0] + 0.5f) * v;
			pixels[i * 4 + 1] = (data[i * 4 + 1] + 0.5f) * v;
			pixels[i * 4 + 2] = (data[i * 4 + 2] + 0.5f) * v;
			pixels[i * 4 + 3] = 1.0f;
		}
	}

	// Clean up
	delete[] data;

	// Success, so create the texture
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	D3D11_SUBRESOURCE_DATA subData = {};
	subData.pSysMem = pixels;
	subData.SysMemPitch = width * 4 * sizeof(float);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&desc, &subData, texture.GetAddressOf());

	// Save to delete pixels
	delete[] pixels;

	// Create the SRV for the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), 0, srv.GetAddressOf());

	// Now that the texture exists, we need to convert to a cube map
	
	// Determine how many mip levels we'll need
	int mips = max((int)(log2(height)) + 1, 1); // Add 1 for 1x1

	// Describe the resource for the cube map, which is simply 
	// a "texture 2d array".  This is a special GPU resource format, 
	// NOT just a C++ array of textures!!!
	D3D11_TEXTURE2D_DESC cubeDesc = {};
	cubeDesc.ArraySize = 6; // Cube map!
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // We'll be using as a texture in a shader
	cubeDesc.CPUAccessFlags = 0; // No read back
	cubeDesc.Format = desc.Format; // Match the loaded texture's color format
	cubeDesc.Width = height;  // Match the size
	cubeDesc.Height = height; // Match the size
	cubeDesc.MipLevels = mips;
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS; // This should be treated as a CUBE, not 6 separate textures
	cubeDesc.Usage = D3D11_USAGE_DEFAULT; // Standard usage
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the actual texture resource
	Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeMapTexture;
	device->CreateTexture2D(&cubeDesc, 0, cubeMapTexture.GetAddressOf());

	// Make a temp sampler
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	SimplePixelShader* ps = this->GetPixelShader("EquirectToCubePS.cso");
	ps->SetShader();
	ps->SetShaderResourceView("Pixels", srv);
	ps->SetSamplerState("BasicSampler", sampler);

	SimpleVertexShader* vs = this->GetVertexShader("FullscreenVS.cso");
	vs->SetShader();

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// == Save previous DX states ===================================

	// Save current render target and depth buffer
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> prevRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> prevDSV;
	context->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

	// Save current viewport
	unsigned int vpCount = 1;
	D3D11_VIEWPORT prevVP = {};
	context->RSGetViewports(&vpCount, &prevVP);

	// Make sure the viewport matches the texture size
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)height;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);
	

	// Loop through the six cubemap faces and fill them
	for (int face = 0; face < 6; face++)
	{
		// Make a render target view for this face
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;	// This points to a Texture2D Array
		rtvDesc.Texture2DArray.ArraySize = 1;			// How much of the array do we need access to?
		rtvDesc.Texture2DArray.FirstArraySlice = face;	// Which texture are we rendering into?
		rtvDesc.Texture2DArray.MipSlice = 0;			// Which mip of that texture are we rendering into?
		rtvDesc.Format = desc.Format;				// Same format as texture

		// Create the render target view itself
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		device->CreateRenderTargetView(cubeMapTexture.Get(), &rtvDesc, rtv.GetAddressOf());

		// Clear and set this render target
		float black[4] = {}; // Initialize to all zeroes
		context->ClearRenderTargetView(rtv.Get(), black);
		context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

		// Per-face shader data and copy
		ps->SetInt("faceIndex", face);
		ps->CopyAllBufferData();

		// Render exactly 3 vertices
		context->Draw(3, 0);

		// Ensure we flush the graphics pipe to so that we don't cause 
		// a hardware timeout which can result in a driver crash
		// NOTE: This might make C++ sit and wait for a sec!  Better than a crash!
		context->Flush();
	}

	// Create the final SRV for the cube map
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeSRV;
	device->CreateShaderResourceView(cubeMapTexture.Get(), 0, cubeSRV.GetAddressOf());

	// Restore the old render target and viewport
	context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
	context->RSSetViewports(1, &prevVP);

	// Now that the first mip has data, generate the rest of the mip levels
	context->GenerateMips(cubeSRV.Get());

	// Add to the dictionary
	textures.insert({ filename, cubeSRV });
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
	unsigned int dataSize = width * height * 4;
	unsigned char* pixels = new unsigned char[dataSize];
	for (int i = 0; i < width * height * 4;)
	{
		pixels[i++] = (unsigned char)(color.x * 255);
		pixels[i++] = (unsigned char)(color.y * 255);
		pixels[i++] = (unsigned char)(color.z * 255);
		pixels[i++] = (unsigned char)(color.w * 255);
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
	data.pSysMem = pixels;
	data.SysMemPitch = sizeof(unsigned char) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// All done with pixel array
	delete[] pixels;

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
