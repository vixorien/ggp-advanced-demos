
cbuffer externalData : register(b0)
{
	// Needed for specular (reflection) calculation
	float3 CameraPosition;

	// The number of mip levels in the specular IBL map
	int SpecIBLTotalMipLevels;

	// Intensity factor for IBL
	float IBLIntensity;
}

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D GBufferAlbedo			: register(t0);
Texture2D GBufferNormals		: register(t1);
Texture2D GBufferDepth			: register(t2);
Texture2D GBufferMetalRough		: register(t3);

Texture2D LightBuffer			: register(t4);

// IBL (indirect PBR) textures
Texture2D BrdfLookUpMap			: register(t5);
TextureCube IrradianceIBLMap	: register(t6);
TextureCube SpecularIBLMap		: register(t7);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	float3 pixelIndex = float3(input.position.xy, 0);
	//float3 surfaceColor = GBufferAlbedo.Load(pixelIndex).rgb;
	//float3 normal = normalize(GBufferNormals.Load(pixelIndex).rgb);
	//float  depth = GBufferDepth.Load(pixelIndex).r;
	//float3 metalRough = GBufferMetalRough.Load(pixelIndex).rgb;

	//// Calculate requisite reflection vectors
	//float3 viewToCam = normalize(CameraPosition - input.worldPos);
	//float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	//float NdotV = saturate(dot(input.normal, viewToCam));

	//// Indirect lighting
	//float3 indirectDiffuse = IndirectDiffuse(IrradianceIBLMap, BasicSampler, input.normal) * IBLIntensity;
	//float3 indirectSpecular = IndirectSpecular(
	//	SpecularIBLMap, SpecIBLTotalMipLevels,
	//	BrdfLookUpMap, ClampSampler, // MUST use the clamp sampler here!
	//	viewRefl, NdotV,
	//	roughness, specColor) * IBLIntensity;

	//// Balance indirect diff/spec
	//float3 balancedIndirectDiff = DiffuseEnergyConserve(indirectDiffuse, specColor, metal) * surfaceColor.rgb;

	float3 totalColor =  LightBuffer.Load(pixelIndex).rgb;
	return float4(totalColor, 1);
}