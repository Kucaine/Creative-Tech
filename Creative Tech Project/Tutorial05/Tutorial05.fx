//--------------------------------------------------------------------------------------
// File: Tutorial04.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;
	float2 Centre;
}

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

float4 VS( float4 Pos : POSITION ) : SV_POSITION
{
    return Pos;
}
//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( VS_OUTPUT input ) : SV_Target
{
	float amplitude = 1;
	//float2 centre = float2(400,300);
	float2 spread = float2(1,1);

	float2 Gaussian;
	float finalGauss;
	Gaussian.x = ((input.Pos.x - Centre.x) * (input.Pos.x - Centre.x) / (2 * (spread.x * spread.x)));
	Gaussian.y = ((input.Pos.y - Centre.y) * (input.Pos.y - Centre.y) / (2 * (spread.y * spread.y)));

	finalGauss = amplitude * exp (-(Gaussian.x + Gaussian.y));

	float4 colour = float4(0.0f,0.0f,0.0f,1.0f);
	
	//colour.r = input.Pos.y / 6;
	
    return finalGauss;
}
