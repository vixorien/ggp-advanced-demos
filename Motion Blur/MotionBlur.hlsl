
cbuffer externalData : register(b0)
{
	float2 pixelSize;
	float motionBlurScale;
	float frameRateFix;
	int motionBlurEnabled;

};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D Pixels				: register(t0);
Texture2D Velocities			: register(t1);
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);


float4 main(VertexToPixel input) : SV_TARGET
{
	// Early out for no blur
	if (!motionBlurEnabled)
	{
		return Pixels.Sample(ClampSampler, input.uv);
	}

	// Gaussian weights and associated offsets (in pixels)
	// Weight values from: http://dev.theomader.com/gaussian-kernel-calculator/
	//  using a Sigma of 4 and 15 for the kernel size
	#define NUM_SAMPLES 15
	const float weights[NUM_SAMPLES] = { 0.023089, 0.034587, 0.048689, 0.064408, 0.080066, 0.093531, 0.102673, 0.105915, 0.102673, 0.093531, 0.080066, 0.064408, 0.048689, 0.034587, 0.023089 };
	const float offsets[NUM_SAMPLES] = { -13.5f, -11.5f, -9.5f, -7.5f, -5.5f, -3.5f, -1.5f, 0, 1.5f, 3.5f, 5.5f, 7.5f, 9.5f, 11.5f, 13.5f};

	// Sample the velocity buffer to get the screen space blur direction
	float2 velocity = Velocities.Sample(ClampSampler, input.uv).rg;

	// What is the offset of a single pixel in the desired direction in UV space
	float2 uvOffset = velocity * pixelSize * motionBlurScale * frameRateFix;

	// Loop through offsets and sample
	float4 colorTotal = float4(0, 0, 0, 0);
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		// The UV of the neighboring pixel we want to sample
		float2 uv = input.uv + (uvOffset * offsets[i]);
		colorTotal += Pixels.Sample(ClampSampler, uv) * weights[i];
	}

	// Final color
	return float4(colorTotal.rgb, 1);
}