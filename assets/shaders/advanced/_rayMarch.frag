#version 460

#define PI 3.14159265359
#define MAX_NEIGHBOURS 30
#define NEIGHBOURHOOD 0.25

#define MAX_STEPS 16
#define STEP_SIZE 1

#define ISO 10

// --------------------------------------------------------------------

layout (std140, binding = 0) uniform UNIFORMS
{
	mat4 ViewProjectionInv;
	vec2 Resolution;
} Uniforms;

layout (std140, binding = 1) readonly buffer PARTICLES
{
	int NumParticles;
	vec3 Positions[];
} Particles;

layout (input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput Depth;

layout (location = 2) out vec4 Position;
layout (location = 3) out vec4 Normal;

// --------------------------------------------------------------------

float W_isotropic(float r)
{
	// equation (7) in the paper
	// SplishSplash file: SPHKernels.h

	float res = 0.0;
	const float q = r / NEIGHBOURHOOD;

	const float k = 8 / (PI * NEIGHBOURHOOD * NEIGHBOURHOOD * NEIGHBOURHOOD);

#ifndef NO_DISTANCE_TEST
	if (q <= 1.0)
#endif
	{
		if (q <= 0.5)
		{
			const float q2 = q * q;
			const float q3 = q2 * q;
			res = k * (6 * q3 - 6 * q2 + 1);
		}
		else res = k * 2 * pow(1 - q , 3);
	}

	return res;
}

float computeDensity(vec3 p)
{
	float density = 0;

	for (int i = 0; i < Particles.NumParticles; ++i)
		density += W_isotropic(distance(Particles.Positions[i], p));

	return density;
}

// --------------------------------------------------------------------

vec2 ClipCoords = 2 * gl_FragCoord.xy / Uniforms.Resolution - 1;

vec3 positionFromDepth()
{
	float z = subpassLoad(Depth).r;
	vec4 world = Uniforms.ViewProjectionInv * vec4(ClipCoords, z, 1);

	return world.xyz / world.w;
}

void main()
{
	//Position = vec4(vec3(subpassLoad(Depth).r), 1);
	Position = vec4(1);
	return;

	// compute ray direction
	vec4 r = Uniforms.ViewProjectionInv * vec4(ClipCoords, 1, 1);
	vec3 direction = r.xyz / r.w;

	// compute starting position
	vec3 p = positionFromDepth();

	for (int i = 0; i < MAX_STEPS; ++i)
	{
		// compute density
		float density = computeDensity(p);

		// found iso-surface?
		if (density > ISO)
		{
			// write output
			Position.xyz = p;
			// Normal = ...

			return;
		}

		// step
		p += STEP_SIZE * direction;
	}

	Position = vec4(0);
}
