#include "Emitter.h"

using namespace DirectX;

Emitter::Emitter(
	int maxParticles,
	int particlesPerSecond,
	float lifetime,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
	SimpleVertexShader* vs,
	SimplePixelShader* ps,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture,
	float startSize,
	float endSize,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	DirectX::XMFLOAT3 emitterPosition,
	DirectX::XMFLOAT3 positionRandomRange,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 velocityRandomRange,
	DirectX::XMFLOAT3 emitterAcceleration,
	DirectX::XMFLOAT2 rotationStartMinMax,
	DirectX::XMFLOAT2 rotationEndMinMax,
	unsigned int spriteSheetWidth,
	unsigned int spriteSheetHeight,
	float spriteSheetSpeedScale)
	:
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	lifetime(lifetime),
	context(context),
	vs(vs),
	ps(ps),
	texture(texture),
	startSize(startSize),
	endSize(endSize),
	startColor(startColor),
	endColor(endColor),
	emitterPosition(emitterPosition),
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
	// Calculate emission rate
	secondsPerParticle = 1.0f / particlesPerSecond;
	
	// Set up emission and lifetime stats
	timeSinceLastEmit = 0.0f;
	livingParticleCount = 0;
	indexFirstAlive = 0;
	indexFirstDead = 0;

	// Set up the particle array
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	// Create an index buffer for particle drawing
	// indices as if we had two triangles per particle
	unsigned int* indices = new unsigned int[maxParticles * 6];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
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

	// Make a dynamic buffer to hold all particle data on GPU
	// Note: We'll be overwriting this every frame with new lifetime data
	D3D11_BUFFER_DESC allParticleBufferDesc = {};
	allParticleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	allParticleBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	allParticleBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	allParticleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	allParticleBufferDesc.StructureByteStride = sizeof(Particle);
	allParticleBufferDesc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&allParticleBufferDesc, 0, particleDataBuffer.GetAddressOf());
	
	// Create an SRV that points to a structured buffer of particles
	// so we can grab this data in a vertex shader
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleDataBuffer.Get(), &srvDesc, particleDataSRV.GetAddressOf());

}

Emitter::~Emitter()
{
	// Clean up the particle array
	delete[] particles;
}


void Emitter::Update(float dt, float currentTime)
{
	// Anything to update?
	if (livingParticleCount > 0)
	{
		// Update all particles - Check cyclic buffer first
		if (indexFirstAlive < indexFirstDead)
		{
			// First alive is BEFORE first dead, so the "living" particles are contiguous
			// 
			// 0 -------- FIRST ALIVE ----------- FIRST DEAD -------- MAX
			// |    dead    |            alive       |         dead    |

			// First alive is before first dead, so no wrapping
			for (int i = indexFirstAlive; i < indexFirstDead; i++)
				UpdateSingleParticle(currentTime, i);
		}
		else if (indexFirstDead < indexFirstAlive)
		{
			// First alive is AFTER first dead, so the "living" particles wrap around
			// 
			// 0 -------- FIRST DEAD ----------- FIRST ALIVE -------- MAX
			// |    alive    |            dead       |         alive   |

			// Update first half (from firstAlive to max particles)
			for (int i = indexFirstAlive; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);

			// Update second half (from 0 to first dead)
			for (int i = 0; i < indexFirstDead; i++)
				UpdateSingleParticle(currentTime, i);
		}
		else
		{
			// First alive is EQUAL TO first dead, so they're either all alive or all dead
			// - Since we know at least one is alive, they should all be
			//
			//            FIRST ALIVE
			// 0 -------- FIRST DEAD -------------------------------- MAX
			// |    alive     |                   alive                |
			for (int i = 0; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);
		}
	}

	// Add to the time
	timeSinceLastEmit += dt;

	// Enough time to emit?
	while (timeSinceLastEmit > secondsPerParticle)
	{
		EmitParticle(currentTime);
		timeSinceLastEmit -= secondsPerParticle;
	}

	// Now that we have emit and updated all particles for this frame, 
	// we can copy them to the GPU as either one big chunk or two smaller chunks

	// Map the buffer
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	// How are living particles arranged in the buffer?
	if (indexFirstAlive < indexFirstDead)
	{
		// Only copy from FirstAlive -> FirstDead
		memcpy(
			mapped.pData, // Destination = start of particle buffer
			particles + indexFirstAlive, // Source = particle array, offset to first living particle
			sizeof(Particle) * livingParticleCount); // Amount = number of particles (measured in BYTES!)
	}
	else
	{
		// Copy from 0 -> FirstDead 
		memcpy(
			mapped.pData, // Destination = start of particle buffer
			particles, // Source = start of particle array
			sizeof(Particle) * indexFirstDead); // Amount = particles up to first dead (measured in BYTES!)

		// ALSO copy from FirstAlive -> End
		memcpy(
			(void*)((Particle*)mapped.pData + indexFirstDead), // Destination = particle buffer, AFTER the data we copied in previous memcpy()
			particles + indexFirstAlive,  // Source = particle array, offset to first living particle
			sizeof(Particle) * (maxParticles - indexFirstAlive)); // Amount = number of living particles at end of array (measured in BYTES!)
	}

	// Unmap now that we're done copying
	context->Unmap(particleDataBuffer.Get(), 0);
}

void Emitter::UpdateSingleParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;

	// Update and check for death
	if (age >= lifetime)
	{
		// Recent death, so retire by moving alive count (and wrap)
		indexFirstAlive++;
		indexFirstAlive %= maxParticles;
		livingParticleCount--;
	}
}

void Emitter::EmitParticle(float currentTime)
{
	// Any left to spawn?
	if (livingParticleCount == maxParticles)
		return;

	// Which particle is spawning?
	int spawnedIndex = indexFirstDead;

	// Update the spawn time
	particles[spawnedIndex].EmitTime = currentTime;

	// Adjust the particle start position based on the random range (box shape)
	particles[spawnedIndex].StartPosition = emitterPosition;
	particles[spawnedIndex].StartPosition.x += positionRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartPosition.y += positionRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartPosition.z += positionRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust particle start velocity based on random range
	particles[spawnedIndex].StartVelocity = startVelocity;
	particles[spawnedIndex].StartVelocity.x += velocityRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartVelocity.y += velocityRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartVelocity.z += velocityRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust start and end rotation values based on range
	particles[spawnedIndex].StartRotation = RandomRange(rotationStartMinMax.x, rotationStartMinMax.y);
	particles[spawnedIndex].EndRotation = RandomRange(rotationEndMinMax.x, rotationEndMinMax.y);

	// Increment the first dead particle (since it's now alive)
	indexFirstDead++;
	indexFirstDead %= maxParticles; // Wrap

	// One more living particle
	livingParticleCount++;
}

void Emitter::Draw(Camera* camera, float currentTime)
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

	// Shader setup
	vs->SetShader();
	ps->SetShader();

	// SRVs - Particle data in VS and texture in PS
	vs->SetShaderResourceView("ParticleData", particleDataSRV);
	ps->SetShaderResourceView("Texture", texture);

	// Vertex data
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat("lifetime", lifetime);
	vs->SetFloat3("acceleration", emitterAcceleration);
	vs->SetFloat("startSize", startSize);
	vs->SetFloat("endSize", endSize);
	vs->SetFloat4("startColor", startColor);
	vs->SetFloat4("endColor", endColor);
	vs->SetInt("spriteSheetWidth", spriteSheetWidth);
	vs->SetInt("spriteSheetHeight", spriteSheetHeight);
	vs->SetFloat("spriteSheetFrameWidth", spriteSheetFrameWidth);
	vs->SetFloat("spriteSheetFrameHeight", spriteSheetFrameHeight);
	vs->SetFloat("spriteSheetSpeedScale", spriteSheetSpeedScale);
	vs->CopyAllBufferData();

	// Now that all of our data is in the beginning of the particle buffer,
	// we can simply draw the correct amount of living particle indices.
	// Each particle = 4 vertices = 6 indices for a quad
	context->DrawIndexed(livingParticleCount * 6, 0, 0);
}

