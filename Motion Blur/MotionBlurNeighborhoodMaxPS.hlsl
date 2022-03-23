
// Based on: https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

Texture2D VelocityTileMax : register(t0);

float4 main(VertexToPixel input) : SV_TARGET
{
	// This pixel in the downsampled texture
	int2 thisIndex = (int2)input.position.xy; // Post-raster position is in (0-w, 0-h)

	float2 maxVelocity = float2(0, 0);
	float maxMagnitude = 0;

	// Get neighborhood max velocity in 3x3 grid
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			// Index of pixel in velocity buffer
			int2 neighborIndex = thisIndex + int2(x,y);
			float2 velocitySample = VelocityTileMax.Load(int3(neighborIndex, 0)).xy;
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