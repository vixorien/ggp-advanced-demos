

#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	int faceIndex;
	float sampleStepPhi;
	float sampleStepTheta;
	int needsGammaCorrection;
};


struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and samplers
TextureCube EnvironmentMap	: register(t0);
SamplerState BasicSampler	: register(s0);

// http://www.codinglabs.net/article_physically_based_rendering.aspx
float4 main(VertexToPixel input) : SV_TARGET
{
	// Get a -1 to 1 range on x/y
	float2 o = input.uv * 2 - 1;

	// Tangent basis
	float3 xDir, yDir, zDir;

	// Figure out the z ("normal" of this pixel)
	switch (faceIndex)
	{
	default:
	case 0: zDir = float3(+1, -o.y, -o.x); break;
	case 1: zDir = float3(-1, -o.y, +o.x); break;
	case 2: zDir = float3(+o.x, +1, +o.y); break;
	case 3: zDir = float3(+o.x, -1, -o.y); break;
	case 4: zDir = float3(+o.x, -o.y, +1); break;
	case 5: zDir = float3(-o.x, -o.y, -1); break;
	}
	zDir = normalize(zDir);

	// Calculate the tangent and bitangent
	xDir = normalize(cross(float3(0, 1, 0), zDir));
	yDir = normalize(cross(zDir, xDir));

	// Total color (to be averaged at the end)
	float3 totalColor = float3(0, 0, 0);
	int sampleCount = 0;

	// Variables for various sin/cos values
	float sinT, cosT, sinP, cosP;


	// Loop around the hemisphere (360 degrees)
	for (float phi = 0.0f; phi < TWO_PI; phi += sampleStepPhi)
	{
		// Grab the sin and cos of phi
		sincos(phi, sinP, cosP);

		// Loop down the hemisphere (90 degrees)
		for (float theta = 0.0f; theta < PI_OVER_2; theta += sampleStepTheta)
		{
			// Get the sin and cos of theta
			sincos(theta, sinT, cosT);

			// Get an X/Y/Z direction from the polar coords
			float3 hemisphereDir = float3(sinT * cosP, sinT * sinP, cosT);

			// Change to world space based on this pixel's direction
			hemisphereDir =
				hemisphereDir.x * xDir +
				hemisphereDir.y * yDir +
				hemisphereDir.z * zDir;

			// Need to adjust sample mip level due to potential HDR skybox.  See:
			// - https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/
			// - https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling


			// Sample in that direction
			float3 samp = EnvironmentMap.Sample(BasicSampler, hemisphereDir).rgb;
			samp = needsGammaCorrection == 1 ? pow(samp, 2.2f) : samp;
			totalColor += cosT * sinT * samp;
			sampleCount++;
		}
	}

	float3 finalColor = PI * totalColor / sampleCount;
	return float4(pow(abs(finalColor), 1.0f / 2.2f), 1);
}