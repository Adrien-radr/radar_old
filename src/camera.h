#pragma once
#include "common/common.h"

struct Camera
{
	void Update( float dt );

	vec3f	position,
		target,
		up,
		forward,
		right;

	f32		translationSpeed,
		rotationSpeed;
	f32		speedMult;

	f32		dist;
	f32		theta,
		phi;

	bool	hasMoved;
	int		speedMode;	// -1 : slower, 0 : normal, 1 : faster
	bool	freeflyMode;
	vec2i	lastMousePos; // mouse position before activating freeflyMode
};