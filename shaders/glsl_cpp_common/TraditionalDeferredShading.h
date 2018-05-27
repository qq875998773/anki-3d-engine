// Copyright (C) 2009-2018, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_SHADERS_GLSL_CPP_COMMON_TRADITIONAL_DEFERRED_SHADING_H
#define ANKI_SHADERS_GLSL_CPP_COMMON_TRADITIONAL_DEFERRED_SHADING_H

#include <shaders/glsl_cpp_common/Common.h>

ANKI_BEGIN_NAMESPACE

struct DeferredPointLightUniforms
{
	Vec4 m_inputTexUvScaleAndOffset; // Use this to get the correct face UVs
	Mat4 m_invViewProjMat;
	Vec4 m_camPosPad1;

	// Light props
	Vec4 m_posRadius; // xyz: Light pos in world space. w: The -1/radius
	Vec4 m_diffuseColorPad1; // xyz: diff color
};

struct DeferredSpotLightUniforms
{
	Vec4 m_inputTexUvScaleAndOffset; // Use this to get the correct face UVs
	Mat4 m_invViewProjMat;
	Vec4 m_camPosPad1;

	// Light props
	Vec4 m_posRadius; // xyz: Light pos in world space. w: The -1/radius
	Vec4 m_diffuseColorOuterCos; // xyz: diff color, w: outer cosine of spot
	Vec4 m_lightDirInnerCos; // xyz: light dir, w: inner cosine of spot
};

ANKI_END_NAMESPACE

#endif