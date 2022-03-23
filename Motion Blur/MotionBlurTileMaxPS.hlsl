
// Based on: https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf

cbuffer externalData : register(b0)
{
	int motionBlurMax;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

Texture2D Velocities			: register(t0);

float4 main(VertexToPixel input) : SV_TARGET
{
	// This pixel in the downsampled texture
	int2 thisIndex = (int2)input.position.xy; // Post-raster position is in (0-w, 0-h)

	float2 maxVelocity = float2(0, 0);
	float maxMagnitude = 0;

	// Perform NxN downsample, storing max of each tile
	int2 tileStartIndex = ((int2)thisIndex) * motionBlurMax;
	for (int x = 0; x < motionBlurMax; x++)
	{
		for (int y = 0; y < motionBlurMax; y++)
		{
			// Index of pixel in velocity buffer
			int2 velocityIndex = tileStartIndex + int2(x,y);
			float2 velocitySample = Velocities.Load(int3(velocityIndex, 0)).xy;
			float sampleMag = length(velocitySample);

			if (sampleMag > maxMagnitude)
			{
				maxVelocity = velocitySample;
				maxMagnitude = sampleMag;
			}
		}
	}

	// Store the final maximum
	return float4(maxVelocity, 0, 0);
}