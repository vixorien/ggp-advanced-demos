#pragma once

#include <DirectXMath.h>
#include <string>
#include "../../Common/json/json.hpp"

// This define should match your
// MAX_LIGHTS definition in your shader(s)
#define MAX_LIGHTS 128

// Light types
// Must match definitions in shader
#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

// Light struct - must match GPU definition!
struct Light
{
	int					Type;
	DirectX::XMFLOAT3	Direction;	// 16 bytes

	float				Range;
	DirectX::XMFLOAT3	Position;	// 32 bytes

	float				Intensity;
	DirectX::XMFLOAT3	Color;		// 48 bytes

	float				SpotFalloff;
	DirectX::XMFLOAT3	Padding;	// 64 bytes

	// -----------------------------------------
	// Static helper for parsing a light from a json
	// representation, usually from a scene file
	// -----------------------------------------
	static Light Parse(nlohmann::json jsonLight)
	{
		// Set defaults
		Light light = {};

		// Check data
		if (jsonLight.contains("type"))
		{
			if (jsonLight["type"].get<std::string>() == "directional") light.Type = LIGHT_TYPE_DIRECTIONAL;
			else if (jsonLight["type"].get<std::string>() == "point") light.Type = LIGHT_TYPE_POINT;
			else if (jsonLight["type"].get<std::string>() == "spot") light.Type = LIGHT_TYPE_SPOT;
		}

		if (jsonLight.contains("direction") && jsonLight["direction"].size() == 3)
		{
			light.Direction.x = jsonLight["direction"][0].get<float>();
			light.Direction.y = jsonLight["direction"][1].get<float>();
			light.Direction.z = jsonLight["direction"][2].get<float>();
		}

		if (jsonLight.contains("position") && jsonLight["position"].size() == 3)
		{
			light.Position.x = jsonLight["position"][0].get<float>();
			light.Position.y = jsonLight["position"][1].get<float>();
			light.Position.z = jsonLight["position"][2].get<float>();
		}

		if (jsonLight.contains("color") && jsonLight["color"].size() == 3)
		{
			light.Color.x = jsonLight["color"][0].get<float>();
			light.Color.y = jsonLight["color"][1].get<float>();
			light.Color.z = jsonLight["color"][2].get<float>();
		}

		if (jsonLight.contains("intensity")) light.Intensity = jsonLight["intensity"].get<float>();
		if (jsonLight.contains("range")) light.Range = jsonLight["range"].get<float>();
		if (jsonLight.contains("spotFalloff")) light.SpotFalloff = jsonLight["spotFalloff"].get<float>();

		return light;
	}
};