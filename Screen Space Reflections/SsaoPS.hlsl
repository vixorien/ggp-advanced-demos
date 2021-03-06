
cbuffer externalData : register(b0)
{
	matrix invViewMatrix;
	matrix invProjMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;

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
	uv.y = 1.0f - uv.y; // Gotta do it this way
	uv = uv * 2.0f - 1.0f;
	//uv.y *= -1.0f; // Flip y due to UV <--> NDC  // NOPE
	float4 screenPos = float4(uv, depth, 1.0f);

	// Back to view space
	float4 viewPos = mul(invProjMatrix, screenPos);
	return viewPos.xyz / viewPos.w;
}


float4 main(VertexToPixel input) : SV_TARGET
{
	// Sample depth first and early out for sky box
	float pixelDepth = Depths.Sample(ClampSampler, input.uv).r;
	if(pixelDepth == 1.0f)
		return float4(1,1,1,1);

	// Get the view space position of this pixel
	float3 pixelPositionViewSpace = ViewSpaceFromDepth(pixelDepth, input.uv);

	// Assuming random texture is 4x4 and holds float values.
	float3 randomDir = Random.Sample(BasicSampler, input.uv * randomTextureScreenScale).xyz;

	// Sample normal and convert to view space
	float3 normal = Normals.Sample(BasicSampler, input.uv).xyz;
	normal = normalize(mul((float3x3)viewMatrix, normal));
	
	// Calculate TBN matrix
	float3 tangent = normalize(randomDir - normal * dot(randomDir, normal));
	float3 bitangent = cross(tangent, normal);
	float3x3 TBN = float3x3(tangent, bitangent, normal);

	// Loop and handle all samples
	float ao = 0.0f;
	for (int i = 0; i < ssaoSamples; i++)
	{
		// Rotate the offset and apply to position
		float4 samplePosView = float4(
			pixelPositionViewSpace + mul(offsets[i].xyz, TBN) * ssaoRadius, 1);

		// Get the UV coord of this position
		float4 samplePosScreen = mul(projectionMatrix, samplePosView);
		samplePosScreen.xyz /= samplePosScreen.w;
		samplePosScreen.xy = samplePosScreen.xy * 0.5f + 0.5f;
		samplePosScreen.y = 1.0f - samplePosScreen.y;

		// Sample the this nearby depth
		float sampleDepth = Depths.SampleLevel(ClampSampler, samplePosScreen.xy, 0).r;
		float sampleZ = ViewSpaceFromDepth(sampleDepth, samplePosScreen.xy).z;

		float rangeCheck = smoothstep(0.0f, 1.0f, ssaoRadius / abs(pixelPositionViewSpace.z - sampleZ));
		ao += (sampleZ < samplePosView.z ? rangeCheck : 0.0f);
	}

	ao = 1.0f - ao / ssaoSamples;


	return float4(ao.rrr, 1);
}