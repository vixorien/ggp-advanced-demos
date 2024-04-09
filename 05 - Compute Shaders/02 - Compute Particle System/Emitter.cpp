#include "Emitter.h"

using namespace DirectX;

Emitter::Emitter(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	std::shared_ptr<Material> material,
	std::shared_ptr<SimpleComputeShader> emitCS,
	std::shared_ptr<SimpleComputeShader> updateCS,
	std::shared_ptr<SimpleComputeShader> deadListInitCS,
	std::shared_ptr<SimpleComputeShader> copyDrawCountCS,
	int maxParticles,
	int particlesPerSecond,
	float lifetime,
	float startSize,
	float endSize,
	bool constrainYAxis,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	DirectX::XMFLOAT3 emitterPosition,
	DirectX::XMFLOAT3 positionRandomRange,
	DirectX::XMFLOAT2 rotationStartMinMax,
	DirectX::XMFLOAT2 rotationEndMinMax,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 velocityRandomRange,
	DirectX::XMFLOAT3 emitterAcceleration,
	unsigned int spriteSheetWidth,
	unsigned int spriteSheetHeight,
	float spriteSheetSpeedScale)
	:
	device(device),
	material(material),
	emitCS(emitCS),
	updateCS(updateCS),
	deadListInitCS(deadListInitCS),
	copyDrawCountCS(copyDrawCountCS),
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	lifetime(lifetime),
	startSize(startSize),
	endSize(endSize),
	constrainYAxis(constrainYAxis),
	startColor(startColor),
	endColor(endColor),
	positionRandomRange(positionRandomRange),
	startVelocity(startVelocity),
	velocityRandomRange(velocityRandomRange),
	emitterAcceleration(emitterAcceleration),
	rotationStartMinMax(rotationStartMinMax),
	rotationEndMinMax(rotationEndMinMax),
	spriteSheetWidth(max(spriteSheetWidth, 1)),
	spriteSheetHeight(max(spriteSheetHeight, 1)),
	spriteSheetFrameWidth(1.0f / spriteSheetWidth),
	spriteSheetFrameHeight(1.0f / spriteSheetHeight),
	spriteSheetSpeedScale(spriteSheetSpeedScale)
{
	// Grab the context from the device
	device->GetImmediateContext(context.GetAddressOf());

	// Calculate emission rate
	secondsPerParticle = 1.0f / particlesPerSecond;

	// Set up emission and lifetime stats
	timeSinceLastEmit = 0.0f;

	this->transform.SetPosition(emitterPosition);

	// Set up any one-time GPU buffers (that don't depend on
	// the number of particles in this emitter)
	{
		// DRAW ARGUMENTS ================
		{
			// Buffer
			D3D11_BUFFER_DESC argsDesc = {};
			argsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
			argsDesc.ByteWidth = sizeof(unsigned int) * 5; // Need 5 if using an index buffer!
			argsDesc.CPUAccessFlags = 0;
			argsDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
			argsDesc.StructureByteStride = 0;
			argsDesc.Usage = D3D11_USAGE_DEFAULT;
			device->CreateBuffer(&argsDesc, 0, drawArgsBuffer.GetAddressOf());
			// Must keep buffer ref for indirect draw, so don't release!

			// UAV
			D3D11_UNORDERED_ACCESS_VIEW_DESC argsUAVDesc = {};
			argsUAVDesc.Format = DXGI_FORMAT_R32_UINT; // Actually UINT's in here!
			argsUAVDesc.Buffer.FirstElement = 0;
			argsUAVDesc.Buffer.Flags = 0;  // Nothing special
			argsUAVDesc.Buffer.NumElements = 5; // Need 5 if using an index buffer
			argsUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			device->CreateUnorderedAccessView(drawArgsBuffer.Get(), &argsUAVDesc, drawArgsUAV.GetAddressOf());
		}

		// DEAD LIST COUNTER BUFFER ==================
		{
			// Also create a buffer to hold the counter of the dead list (copied each frame)
			D3D11_BUFFER_DESC deadCounterDesc = {};
			deadCounterDesc.Usage = D3D11_USAGE_DEFAULT;
			deadCounterDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			deadCounterDesc.ByteWidth = 16; // Has to be multiple of 16 for cbuffer
			deadCounterDesc.CPUAccessFlags = 0;
			deadCounterDesc.MiscFlags = 0;
			deadCounterDesc.StructureByteStride = 0;
			device->CreateBuffer(&deadCounterDesc, 0, deadListCounterBuffer.GetAddressOf());
		}
	}

	// Create other emitter-specific GPU resources
	CreateGPUResources();
}

Emitter::~Emitter()
{
}

Transform* Emitter::GetTransform() { return &transform; }
std::shared_ptr<Material> Emitter::GetMaterial() { return material; }
void Emitter::SetMaterial(std::shared_ptr<Material> material) { this->material = material; }

void Emitter::CreateGPUResources()
{
	// Delete and release existing resources
	indexBuffer.Reset();

	particlePoolUAV.Reset();
	particlePoolSRV.Reset();

	particleDeadUAV.Reset();

	particleDrawUAV.Reset();
	particleDrawSRV.Reset();
	
	// INDEX BUFFER ==========================
	{
		// Create an index buffer for particle drawing
		// indices as if we had two triangles per particle
		int numIndices = maxParticles * 6;
		unsigned int* indices = new unsigned int[numIndices];
		int indexCount = 0;
		for (int i = 0; i < maxParticles * 4; i += 4) // TODO: Verify these numbers
		{
			indices[indexCount++] = i;
			indices[indexCount++] = i + 1;
			indices[indexCount++] = i + 2;
			indices[indexCount++] = i;
			indices[indexCount++] = i + 2;
			indices[indexCount++] = i + 3;
		}
		D3D11_SUBRESOURCE_DATA indexData = {};
		indexData.pSysMem = indices;

		// Regular (static) index buffer
		D3D11_BUFFER_DESC ibDesc = {};
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibDesc.CPUAccessFlags = 0;
		ibDesc.Usage = D3D11_USAGE_DEFAULT;
		ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
		device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
		delete[] indices; // Sent to GPU already
	}

	// PARTICLE POOL ============
	{
		// Buffer
		Microsoft::WRL::ComPtr<ID3D11Buffer> particlePoolBuffer;
		D3D11_BUFFER_DESC poolDesc = {};
		poolDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		poolDesc.ByteWidth = sizeof(Particle) * maxParticles;
		poolDesc.CPUAccessFlags = 0;
		poolDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		poolDesc.StructureByteStride = sizeof(Particle);
		poolDesc.Usage = D3D11_USAGE_DEFAULT;
		device->CreateBuffer(&poolDesc, 0, particlePoolBuffer.GetAddressOf());

		// UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC poolUAVDesc = {};
		poolUAVDesc.Format = DXGI_FORMAT_UNKNOWN; // Needed for RW structured buffers
		poolUAVDesc.Buffer.FirstElement = 0;
		poolUAVDesc.Buffer.Flags = 0;
		poolUAVDesc.Buffer.NumElements = maxParticles;
		poolUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		device->CreateUnorderedAccessView(particlePoolBuffer.Get(), &poolUAVDesc, particlePoolUAV.GetAddressOf());

		// SRV (for indexing in VS)
		D3D11_SHADER_RESOURCE_VIEW_DESC poolSRVDesc = {};
		poolSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		poolSRVDesc.Buffer.FirstElement = 0;
		poolSRVDesc.Buffer.NumElements = maxParticles;
		// Don't actually set these!  They're union'd with above data, so 
		// it will just overwrite correct values with incorrect values
		//poolSRVDesc.Buffer.ElementOffset;
		//poolSRVDesc.Buffer.ElementWidth;
		poolSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		device->CreateShaderResourceView(particlePoolBuffer.Get(), &poolSRVDesc, particlePoolSRV.GetAddressOf());
	}

	// DEAD LIST ===================
	{
		// Buffer
		Microsoft::WRL::ComPtr<ID3D11Buffer> deadListBuffer;
		D3D11_BUFFER_DESC deadDesc = {};
		deadDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		deadDesc.ByteWidth = sizeof(unsigned int) * maxParticles;
		deadDesc.CPUAccessFlags = 0;
		deadDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		deadDesc.StructureByteStride = sizeof(unsigned int);
		deadDesc.Usage = D3D11_USAGE_DEFAULT;
		device->CreateBuffer(&deadDesc, 0, deadListBuffer.GetAddressOf());

		// UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC deadUAVDesc = {};
		deadUAVDesc.Format = DXGI_FORMAT_UNKNOWN; // Needed for RW structured buffers
		deadUAVDesc.Buffer.FirstElement = 0;
		deadUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND; // Append/Consume
		deadUAVDesc.Buffer.NumElements = maxParticles;
		deadUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		device->CreateUnorderedAccessView(deadListBuffer.Get(), &deadUAVDesc, particleDeadUAV.GetAddressOf());
	}

	// Draw List
	{
		// Buffer
		Microsoft::WRL::ComPtr<ID3D11Buffer> drawListBuffer;
		D3D11_BUFFER_DESC drawDesc = {};
		drawDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		drawDesc.ByteWidth = sizeof(ParticleDrawData) * maxParticles;
		drawDesc.CPUAccessFlags = 0;
		drawDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		drawDesc.StructureByteStride = sizeof(ParticleDrawData);
		drawDesc.Usage = D3D11_USAGE_DEFAULT;
		device->CreateBuffer(&drawDesc, 0, drawListBuffer.GetAddressOf());

		// UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC drawUAVDesc = {};
		drawUAVDesc.Format = DXGI_FORMAT_UNKNOWN; // Needed for RW structured buffers
		drawUAVDesc.Buffer.FirstElement = 0;
		drawUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER; // IncrementCounter() in HLSL
		drawUAVDesc.Buffer.NumElements = maxParticles;
		drawUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		device->CreateUnorderedAccessView(drawListBuffer.Get(), &drawUAVDesc, particleDrawUAV.GetAddressOf());

		// SRV (for indexing in VS)
		D3D11_SHADER_RESOURCE_VIEW_DESC drawSRVDesc = {};
		drawSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		drawSRVDesc.Buffer.FirstElement = 0;
		drawSRVDesc.Buffer.NumElements = maxParticles;
		// Don't actually set these!  They're union'd with above data, so 
		// it will just overwrite correct values with incorrect values
		//drawSRVDesc.Buffer.ElementOffset;
		//drawSRVDesc.Buffer.ElementWidth;
		drawSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		device->CreateShaderResourceView(drawListBuffer.Get(), &drawSRVDesc, particleDrawSRV.GetAddressOf());
	}

	// Populate dead list
	{
		// Launch the dead list init shader
		deadListInitCS->SetInt("MaxParticles", maxParticles);
		deadListInitCS->SetUnorderedAccessView("DeadList", particleDeadUAV);
		deadListInitCS->SetShader();
		deadListInitCS->CopyAllBufferData();
		deadListInitCS->DispatchByThreads(maxParticles, 1, 1);

		// Now copy the counter from the dead list to ensure we
		context->CopyStructureCount(deadListCounterBuffer.Get(), 0, particleDeadUAV.Get());
	}
}


void Emitter::Update(float dt, float currentTime)
{
	// TODO: Reset UAVs?
	ID3D11UnorderedAccessView* none[8] = {};
	context->CSSetUnorderedAccessViews(0, 8, none, 0);

	// Add to the time
	timeSinceLastEmit += dt;

	// EMIT ========================
	if (timeSinceLastEmit > secondsPerParticle)
	{
		// How many are we emitting?
		int emitCount = (int)(timeSinceLastEmit / secondsPerParticle);
		timeSinceLastEmit = fmod(timeSinceLastEmit, secondsPerParticle);

		// Emit an appropriate amount of particles
		emitCS->SetShader();
		emitCS->SetInt("EmitCount", emitCount);
		emitCS->SetFloat("CurrentTime", currentTime);
		emitCS->SetInt("MaxParticles", (int)maxParticles);
		emitCS->SetFloat3("StartPosition", transform.GetPosition());
		emitCS->SetFloat3("StartVelocity", startVelocity);
		emitCS->SetFloat3("PosRandomRange", positionRandomRange);
		emitCS->SetFloat3("VelRandomRange", velocityRandomRange);
		emitCS->SetFloat2("RotStartMinMax", rotationStartMinMax);
		emitCS->SetFloat2("RotEndMinMax", rotationEndMinMax);
		emitCS->CopyAllBufferData();

		emitCS->SetUnorderedAccessView("ParticlePool", particlePoolUAV);
		emitCS->SetUnorderedAccessView("DeadList", particleDeadUAV);
		context->CSSetConstantBuffers(1, 1, deadListCounterBuffer.GetAddressOf()); // Manually setting a whole cbuffer here
		
		emitCS->DispatchByThreads(emitCount, 1, 1);
	}

	// SIMULATE ========================
	{
		// Update
		updateCS->SetShader();
		updateCS->SetFloat("CurrentTime", currentTime);
		updateCS->SetFloat("Lifetime", lifetime);
		updateCS->SetInt("MaxParticles", maxParticles);
		updateCS->SetUnorderedAccessView("ParticlePool", particlePoolUAV);
		updateCS->SetUnorderedAccessView("DeadList", particleDeadUAV);
		updateCS->SetUnorderedAccessView("DrawList", particleDrawUAV, 0); // Reset counter for update!

		updateCS->CopyAllBufferData();
		updateCS->DispatchByThreads(maxParticles, 1, 1);
	}

	// PREPARE DRAW DATA ===============
	{
		// TODO: Necessary?
		// Binding order issues with next stage, so just reset here
		context->CSSetUnorderedAccessViews(0, 8, none, 0);

		// Get draw data
		copyDrawCountCS->SetShader();
		copyDrawCountCS->SetInt("VertsPerParticle", 6);
		copyDrawCountCS->CopyAllBufferData();
		copyDrawCountCS->SetUnorderedAccessView("DrawArgs", drawArgsUAV);
		copyDrawCountCS->SetUnorderedAccessView("DrawList", particleDrawUAV); // Don't reset counter!!!
		copyDrawCountCS->DispatchByThreads(1, 1, 1);

		// TODO: Reset here too?
		context->CSSetUnorderedAccessViews(0, 8, none, 0);

		// Copy dead counter
		context->CopyStructureCount(deadListCounterBuffer.Get(), 0, particleDeadUAV.Get());
	}
}



void Emitter::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime)
{
	// Set up buffers - note that we're NOT using a vertex buffer!
	// When we draw, we'll calculate the number of vertices we expect
	// to have given how many particles are currently alive.  We'll
	// construct the actual vertex data on the fly in the shader.
	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* nullBuffer = 0;
	context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Set particle-specific data and let the
	// material take care of the rest
	material->PrepareMaterial(&transform, camera);

	// Vertex data
	std::shared_ptr<SimpleVertexShader> vs = material->GetVertexShader();
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat("lifetime", lifetime);
	vs->SetFloat3("acceleration", emitterAcceleration);
	vs->SetFloat("startSize", startSize);
	vs->SetFloat("endSize", endSize);
	vs->SetFloat4("startColor", startColor);
	vs->SetFloat4("endColor", endColor);
	vs->SetInt("constrainYAxis", constrainYAxis);
	vs->SetInt("spriteSheetWidth", spriteSheetWidth);
	vs->SetInt("spriteSheetHeight", spriteSheetHeight);
	vs->SetFloat("spriteSheetFrameWidth", spriteSheetFrameWidth);
	vs->SetFloat("spriteSheetFrameHeight", spriteSheetFrameHeight);
	vs->SetFloat("spriteSheetSpeedScale", spriteSheetSpeedScale);
	vs->CopyAllBufferData();

	// Set particle structured buffers in Vertex Shader
	context->VSSetShaderResources(0, 1, particlePoolSRV.GetAddressOf());
	context->VSSetShaderResources(1, 1, particleDrawSRV.GetAddressOf());


	// Draw using indirect args
	context->DrawIndexedInstancedIndirect(drawArgsBuffer.Get(), 0);

	ID3D11ShaderResourceView* none[16] = {};
	context->VSSetShaderResources(0, 16, none);
}

int Emitter::GetParticlesPerSecond()
{
	return particlesPerSecond;
}

void Emitter::SetParticlesPerSecond(int particlesPerSecond)
{
	this->particlesPerSecond = max(1, particlesPerSecond);
	this->secondsPerParticle = 1.0f / particlesPerSecond;
}

int Emitter::GetMaxParticles()
{
	return maxParticles;
}

void Emitter::SetMaxParticles(int maxParticles)
{
	this->maxParticles = max(1, maxParticles);
	CreateGPUResources();

	// Reset emission details
	timeSinceLastEmit = 0.0f;
}


bool Emitter::IsSpriteSheet()
{
	return spriteSheetHeight > 1 || spriteSheetWidth > 1;
}

