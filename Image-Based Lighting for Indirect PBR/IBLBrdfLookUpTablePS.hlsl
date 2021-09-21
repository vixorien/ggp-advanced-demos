
#include "Lighting.hlsli"

#define MAX_SAMPLES 4096


struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};



// Specular G
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float G1_Schlick(float Roughness, float NdotV)
{
	float k = Roughness * Roughness;
	k /= 2.0f; // Schlick-GGX version of k - Used in UE4

	// Staying the same
	return NdotV / (NdotV * (1.0f - k) + k);
}

// Specular G
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float G_Smith(float Roughness, float NdotV, float NdotL)
{
	return G1_Schlick(Roughness, NdotV) * G1_Schlick(Roughness, NdotL);
}


// Convolves the texture cube for a particular roughness and vector
float2 IntegrateBRDF(float roughnessValue, float nDotV)
{
	float3 V;
	V.x = sqrt(1.0f - nDotV * nDotV);
	V.y = 0;
	V.z = nDotV;

	float3 N = float3(0, 0, 1);

	float A = 0;
	float B = 0;

	// Run the calculation MANY times
	//  - 4096 would be an ideal number of times 
	//  - Fewer is faster, but is less accurate
	for (uint i = 0; i < MAX_SAMPLES; i++)
	{
		// Grab this sample
		float2 Xi = Hammersley2d(i, MAX_SAMPLES);
		float3 H = ImportanceSampleGGX(Xi, roughnessValue, N);
		float3 L = 2 * dot(V, H) * H - V;

		float nDotL = saturate(L.z);
		float nDotH = saturate(H.z);
		float vDotH = saturate(dot(V, H));

		// Check N dot L result
		if (nDotL > 0)
		{
			float G = G_Smith(roughnessValue, nDotV, nDotL);
			float G_Vis = G * vDotH / (nDotH * nDotV);
			float Fc = pow(1 - vDotH, 5);
			A += (1 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	// Divide and return result
	return float2(A, B) / MAX_SAMPLES;
}

// All from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float4 main(VertexToPixel input) : SV_TARGET
{
	// Treat the uv range (0-1) as a grid of 
	// roughness and nDotV permutations
	float roughness = input.uv.x;
	float nDotV = input.uv.y;

	// Handle this pixel and save
	float2 brdf = IntegrateBRDF(roughness, nDotV);
	return float4(brdf, 0, 1);
}