
// Based on: https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf

cbuffer externalData : register(b0)
{
	float2 pixelSize;
	float motionBlurScale;
	float frameRateFix;
	int motionBlurEnabled;
	int motionBlurMax;
	int motionBlurSamples;
	float2 screenSize;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D Pixels				: register(t0);
Texture2D Depths				: register(t1);
Texture2D Velocities			: register(t2);
Texture2D VelocityNeighborhoodMax : register(t3);
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);


// Determines if B is closer than A, with a falloff
float SoftDepthCompare(float depthA, float depthB)
{
	const float SOFT_Z_EXTENT = 0.1f; // Test other values?
	return saturate(1.0f - (depthA - depthB) / SOFT_Z_EXTENT);
}


float Cone(float2 X, float2 Y, float2 velocity)
{
	return saturate(1.0f - length(X - Y) / length(velocity));
}

float Cylinder(float2 X, float2 Y, float2 velocity)
{
	return saturate(1.0f - smoothstep(0.95f * length(velocity), 1.05f * length(velocity), length(X - Y)));
}

float4 main(VertexToPixel input) : SV_TARGET
{
	int2 pixelCenter = (int2)input.position.xy;

	// Early out for no blur
	if (!motionBlurEnabled)
	{
		return Pixels.Load(int3(pixelCenter, 0));
	}

	// Sample initial data to determine if there is blur here
	float3 colorCenter = Pixels.Load(int3(pixelCenter, 0)).rgb;
	float2 velocityNeighborhood = VelocityNeighborhoodMax.Sample(ClampSampler, input.uv).rg;
	if (length(velocityNeighborhood) <= 0.5f)
		return float4(colorCenter, 1);

	// Yes there is blur, so sample remaining textures
	float depthCenter = Depths.Load(int3(pixelCenter, 0)).r;
	float2 velocityCenter = Velocities.Load(int3(pixelCenter, 0)).rg;
	float velMag = length(velocityCenter);

	// Need weights and overall samples
	float weight = velMag == 0 ? 1.0f : 1.0f / length(velocityCenter);
	float3 sum = colorCenter * weight;

	// Step size for loop
	float2 stepSizeUV = (velocityNeighborhood / screenSize) / motionBlurSamples;

	// Loop in both directions
	[loop] // Force compiler to loop instead of unroll
	for (int i = -motionBlurSamples; i <= motionBlurSamples; i++)
	{
		// Skip center
		if (i == 0) continue;

		// Calculate UV here
		float2 uvSample = input.uv + stepSizeUV * i;
		int2 pixelSample = (int2)(uvSample * screenSize);

		// Sample data here
		float depthSample = Depths.Sample(ClampSampler, uvSample).r; // BORDER SAMPLER instead?
		float3 colorSample = Pixels.Sample(ClampSampler, uvSample).rgb;
		float2 velocitySample = Velocities.Sample(ClampSampler, uvSample).rg;

		// Determine foreground/background ramp values
		float fore = SoftDepthCompare(depthCenter, depthSample);  // NOTE: Wants linear depth!
		float back = SoftDepthCompare(depthSample, depthCenter);  // NOTE: Wants linear depth!

		// Weight this sample
		float weightSample = 
			// Case 1: Blurry sample in front
			fore * Cone(pixelSample, pixelCenter, velocitySample) +
		
			// Case 2: Behind blurry center
			back * Cone(pixelCenter, pixelSample, velocityCenter) +
		
			// Case 3: Blurry fore and background
			Cylinder(pixelSample, pixelCenter, velocitySample) * Cylinder(pixelCenter, pixelSample, velocityCenter) * 2;

		// Accumulate
		weight += weightSample;
		sum += weightSample * colorSample;
	}

	return float4(sum / weight, 1);
}