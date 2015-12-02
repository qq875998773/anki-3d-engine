// Copyright (C) 2009-2015, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

// Common code for all vertex shaders of FS
#pragma anki include "shaders/MsFsCommon.glsl"

// Global resources
#define LIGHT_SET 1
#define LIGHT_SS_BINDING 0
#define LIGHT_TEX_BINDING 1
#pragma anki include "shaders/LightResources.glsl"
#undef LIGHT_SET
#undef LIGHT_SS_BINDING
#undef LIGHT_TEX_BINDING

// In/out
layout(location = POSITION_LOCATION) in vec3 in_position;
layout(location = SCALE_LOCATION) in float in_scale;
layout(location = ALPHA_LOCATION) in float in_alpha;

layout(location = 0) out vec3 out_vertPosViewSpace;
layout(location = 1) flat out float out_alpha;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

//==============================================================================
#define setPositionVec3_DEFINED
void setPositionVec3(in vec3 pos)
{
	gl_Position = vec4(pos, 1.0);
}

//==============================================================================
#define setPositionVec4_DEFINED
void setPositionVec4(in vec4 pos)
{
	gl_Position = pos;
}

//==============================================================================
#define writePositionMvp_DEFINED
void writePositionMvp(in mat4 mvp)
{
	gl_Position = mvp * vec4(in_position, 1.0);
}

//==============================================================================
#define particle_DEFINED
void particle(in mat4 mvp)
{
	gl_Position = mvp * vec4(in_position, 1);
	out_alpha = in_alpha;
	gl_PointSize =
		in_scale * u_lightingUniforms.rendererSizeTimePad1.x / gl_Position.w;
}

//==============================================================================
#define writeAlpha_DEFINED
void writeAlpha(in float alpha)
{
	out_alpha = alpha;
}

//==============================================================================
#define writeVertPosViewSpace_DEFINED
void writeVertPosViewSpace(in mat4 modelViewMat)
{
	out_vertPosViewSpace = vec3(modelViewMat * vec4(in_position, 1.0));
}
