
cbuffer externalData : register(b0)
{
	float2 pixelSize;
	float2 blurDirection;
	int blurAlpha;
}

// Defines the input to this pixel shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D PixelColors		: register(t0);
SamplerState ClampSampler	: register(s1);


float4 main(VertexToPixel input) : SV_TARGET
{
	#define NUM_SAMPLES 15

	// Gaussian weights and associated offsets (in pixels)
	// Weight values from: http://dev.theomader.com/gaussian-kernel-calculator/
	//  using a Sigma of 4 and 15 for the kernel size
	const float weights[NUM_SAMPLES] = { 0.023089, 0.034587, 0.048689, 0.064408, 0.080066, 0.093531, 0.102673, 0.105915, 0.102673, 0.093531, 0.080066, 0.064408, 0.048689, 0.034587, 0.023089 };
	const float offsets[NUM_SAMPLES] = { -13.5f, -11.5f, -9.5f, -7.5f, -5.5f, -3.5f, -1.5f, 0, 1.5f, 3.5f, 5.5f, 7.5f, 9.5f, 11.5f, 13.5f};

	// Running total of color as we sample neighboring pixels
	float4 colorTotal = float4(0,0,0,0);

	// What is the offset of a single pixel in the desired direction in UV space
	float2 uvOffset = blurDirection * pixelSize;

	// Loop through offsets and sample
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		// The UV of the neighboring pixel we want to sample
		float2 uv = input.uv + (uvOffset * offsets[i]);
		colorTotal += PixelColors.Sample(ClampSampler, uv) * weights[i];
	}

	// Final color
	return float4(colorTotal.rgb, blurAlpha ? colorTotal.a : 1);
}