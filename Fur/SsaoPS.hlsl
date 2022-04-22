
cbuffer externalData : register(b0)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix invProjMatrix;

	float4 offsets[64];
	float ssaoRadius;
	int ssaoSamples;
	float2 randomTextureScreenScale;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D Normals			: register(t0);
Texture2D Depths			: register(t1);
Texture2D Random			: register(t2);
SamplerState BasicSampler	: register(s0);
SamplerState ClampSampler	: register(s1);


float3 ViewSpaceFromDepth(float depth, float2 uv)
{
	// Back to NDCs
	uv.y = 1.0f - uv.y; // Invert Y due to UV <--> NDC diff
	uv = uv * 2.0f - 1.0f;
	float4 screenPos = float4(uv, depth, 1.0f);

	// Back to view space
	float4 viewPos = mul(invProjMatrix, screenPos);
	return viewPos.xyz / viewPos.w;
}

float2 UVFromViewSpacePosition(float3 viewSpacePosition)
{
	// Apply the projection matrix to the view space position then perspective divide
	float4 samplePosScreen = mul(projectionMatrix, float4(viewSpacePosition, 1));
	samplePosScreen.xyz /= samplePosScreen.w;

	// Adjust from NDCs to UV coords (flip the Y!)
	samplePosScreen.xy = samplePosScreen.xy * 0.5f + 0.5f;
	samplePosScreen.y = 1.0f - samplePosScreen.y;
	
	// Return just the UVs
	return samplePosScreen.xy;
}

float4 main(VertexToPixel input) : SV_TARGET
{
	// Sample depth first and early out for sky box
	float pixelDepth = Depths.Sample(ClampSampler, input.uv).r;
	if(pixelDepth == 1.0f)
		return float4(1,1,1,1);

	// Get the view space position of this pixel
	float3 pixelPositionViewSpace = ViewSpaceFromDepth(pixelDepth, input.uv);

	// Assuming random texture is 4x4 and holds float values (already normalized)
	float3 randomDir = Random.Sample(BasicSampler, input.uv * randomTextureScreenScale).xyz;

	// Sample normal and convert to view space
	float3 normal = Normals.Sample(BasicSampler, input.uv).xyz * 2 - 1;
	normal = normalize(mul((float3x3)viewMatrix, normal));
	
	// Calculate TBN matrix
	float3 tangent = normalize(randomDir - normal * dot(randomDir, normal));
	float3 bitangent = cross(tangent, normal);
	float3x3 TBN = float3x3(tangent, bitangent, normal);

	// Loop and total all samples
	float ao = 0.0f;
	for (int i = 0; i < ssaoSamples; i++)
	{
		// Rotate the offset, scale and apply to position
		float3 samplePosView = pixelPositionViewSpace + mul(offsets[i].xyz, TBN) * ssaoRadius;

		// Get the UV coord of this position
		float2 samplePosScreen = UVFromViewSpacePosition(samplePosView);

		// Sample the this nearby depth
		float sampleDepth = Depths.SampleLevel(ClampSampler, samplePosScreen.xy, 0).r;
		float sampleZ = ViewSpaceFromDepth(sampleDepth, samplePosScreen.xy).z;

		// Compare the depths and fade result based on range (so far away objects aren’t occluded)
		float rangeCheck = smoothstep(0.0f, 1.0f, ssaoRadius / abs(pixelPositionViewSpace.z - sampleZ));
		ao += (sampleZ < samplePosView.z ? rangeCheck : 0.0f);
	}

	ao = 1.0f - ao / ssaoSamples;
	return float4(ao.rrr, 1);
}