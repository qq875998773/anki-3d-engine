// Copyright (C) 2009-2019, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include "Importer.h"

namespace anki
{

const char* MATERIAL_TEMPLATE = R"(<!-- This file is auto generated by ExporterMaterial.cpp -->
<material shaderProgram="shaders/GBufferGeneric.glslp">

	<mutators>
		<mutator name="DIFFUSE_TEX" value="%diffTexMutator%"/>
		<mutator name="SPECULAR_TEX" value="%specTexMutator%"/>
		<mutator name="ROUGHNESS_TEX" value="%roughnessTexMutator%"/>
		<mutator name="METAL_TEX" value="%metalTexMutator%"/>
		<mutator name="NORMAL_TEX" value="%normalTexMutator%"/>
		<mutator name="PARALLAX" value="%parallaxMutator%"/>
		<mutator name="EMISSIVE_TEX" value="%emissiveTexMutator%"/>
	</mutators>

	<inputs>
		<input shaderInput="mvp" builtin="MODEL_VIEW_PROJECTION_MATRIX"/>
		<input shaderInput="prevMvp" builtin="PREVIOUS_MODEL_VIEW_PROJECTION_MATRIX"/>
		<input shaderInput="rotationMat" builtin="ROTATION_MATRIX"/>
		<input shaderInput="globalSampler" builtin="GLOBAL_SAMPLER"/>
		%parallaxInput%

		%diff%
		%spec%
		%roughness%
		%metallic%
		%normal%
		%emission%
		%subsurface%
		%height%
	</inputs>
</material>
)";

static std::string replaceAllString(const std::string& str, const std::string& from, const std::string& to)
{
	if(from.empty())
	{
		return str;
	}

	std::string out = str;
	size_t start_pos = 0;
	while((start_pos = out.find(from, start_pos)) != std::string::npos)
	{
		out.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}

	return out;
}

static CString getTextureUri(const cgltf_texture_view& view)
{
	ANKI_ASSERT(view.texture);
	ANKI_ASSERT(view.texture->image);
	ANKI_ASSERT(view.texture->image->uri);
	return view.texture->image->uri;
}

Error Importer::writeMaterial(const cgltf_material& mtl)
{
	StringAuto fname(m_alloc);
	fname.sprintf("%s%s.ankimtl", m_outDir.cstr(), mtl.name);
	ANKI_GLTF_LOGI("Importing material %s", fname.cstr());

	if(!mtl.has_pbr_metallic_roughness)
	{
		ANKI_GLTF_LOGE("Expecting PBR metallic roughness");
		return Error::USER_DATA;
	}

	std::string xml = MATERIAL_TEMPLATE;

	// Diffuse
	if(mtl.pbr_metallic_roughness.base_color_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf("%s%s", m_texrpath.cstr(), getTextureUri(mtl.pbr_metallic_roughness.base_color_texture).cstr());

		xml = replaceAllString(
			xml, "%diff%", "<input shaderInput=\"diffTex\" value=\"" + std::string(uri.cstr()) + "\"/>");
		xml = replaceAllString(xml, "%diffTexMutator%", "1");
	}
	else
	{
		const F32* diffCol = &mtl.pbr_metallic_roughness.base_color_factor[0];

		xml = replaceAllString(xml,
			"%diff%",
			"<input shaderInput=\"diffColor\" value=\"" + std::to_string(diffCol[0]) + " " + std::to_string(diffCol[1])
				+ " " + std::to_string(diffCol[2]) + "\"/>");

		xml = replaceAllString(xml, "%diffTexMutator%", "0");
	}

	// Specular color (freshnel)
	// TODO
	{
		xml = replaceAllString(xml, "%spec%", "<input shaderInput=\"specColor\" value=\"0.04 0.04 0.04\"/>");
		xml = replaceAllString(xml, "%specTexMutator%", "0");
	}

	// Roughness
	if(mtl.pbr_metallic_roughness.metallic_roughness_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf(
			"%s%s", m_texrpath.cstr(), getTextureUri(mtl.pbr_metallic_roughness.metallic_roughness_texture).cstr());

		xml = replaceAllString(
			xml, "%roughness%", "<input shaderInput=\"roughnessTex\" value=\"" + std::string(uri.cstr()) + "\"/>");

		xml = replaceAllString(xml, "%roughnessTexMutator%", "1");
	}
	else
	{
		xml = replaceAllString(xml,
			"%roughness%",
			"<input shaderInput=\"roughness\" value=\"" + std::to_string(mtl.pbr_metallic_roughness.roughness_factor)
				+ "\" />");

		xml = replaceAllString(xml, "%roughnessTexMutator%", "0");
	}

	// Metallic
	if(mtl.pbr_metallic_roughness.metallic_roughness_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf(
			"%s%s", m_texrpath.cstr(), getTextureUri(mtl.pbr_metallic_roughness.metallic_roughness_texture).cstr());

		xml = replaceAllString(
			xml, "%metallic%", "<input shaderInput=\"metallicTex\" value=\"" + std::string(uri.cstr()) + "\"/>");

		xml = replaceAllString(xml, "%metalTexMutator%", "1");
	}
	else
	{
		xml = replaceAllString(xml,
			"%metallic%",
			"<input shaderInput=\"metallic\" value=\"" + std::to_string(mtl.pbr_metallic_roughness.metallic_factor)
				+ "\" />");

		xml = replaceAllString(xml, "%metalTexMutator%", "0");
	}

	// Normal texture
	if(mtl.normal_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf("%s%s", m_texrpath.cstr(), getTextureUri(mtl.normal_texture).cstr());

		xml = replaceAllString(
			xml, "%normal%", "<input shaderInput=\"normalTex\" value=\"" + std::string(uri.cstr()) + "\"/>");

		xml = replaceAllString(xml, "%normalTexMutator%", "1");
	}
	else
	{
		xml = replaceAllString(xml, "%normal%", "");
		xml = replaceAllString(xml, "%normalTexMutator%", "0");
	}

	// Emissive texture
	if(mtl.emissive_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf("%s%s", m_texrpath.cstr(), getTextureUri(mtl.emissive_texture).cstr());

		xml = replaceAllString(
			xml, "%emission%", "<input shaderInput=\"emissiveTex\" value=\"" + std::string(uri.cstr()) + "\"/>");

		xml = replaceAllString(xml, "%emissiveTexMutator%", "1");
	}
	else
	{
		const F32* emissionCol = &mtl.emissive_factor[0];

		xml = replaceAllString(xml,
			"%emission%",
			"<input shaderInput=\"emission\" value=\"" + std::to_string(emissionCol[0]) + " "
				+ std::to_string(emissionCol[1]) + " " + std::to_string(emissionCol[2]) + "\"/>");

		xml = replaceAllString(xml, "%emissiveTexMutator%", "0");
	}

	// Subsurface
	// TODO
	{
		F32 subsurface = 0.0f;

		xml = replaceAllString(
			xml, "%subsurface%", "<input shaderInput=\"subsurface\" value=\"" + std::to_string(subsurface) + "\"/>");
	}

	// Height texture
	// TODO Add native support and not use occlusion map
	if(mtl.occlusion_texture.texture)
	{
		StringAuto uri(m_alloc);
		uri.sprintf("%s%s", m_texrpath.cstr(), getTextureUri(mtl.occlusion_texture).cstr());

		xml = replaceAllString(xml,
			"%height%",
			"<input shaderInput=\"heightTex\" value=\"" + std::string(uri.cstr())
				+ "\"/>\n"
				  "\t\t<input shaderInput=\"heightMapScale\" value=\"0.05\"/>");

		xml = replaceAllString(
			xml, "%parallaxInput%", "<input shaderInput=\"modelViewMat\" builtin=\"MODEL_VIEW_MATRIX\"/>");

		xml = replaceAllString(xml, "%parallaxMutator%", "1");
	}
	else
	{
		xml = replaceAllString(xml, "%height%", "");
		xml = replaceAllString(xml, "%parallaxInput%", "");
		xml = replaceAllString(xml, "%parallaxMutator%", "0");
	}

	// Replace texture extensions with .anki
	xml = replaceAllString(xml, ".tga", ".ankitex");
	xml = replaceAllString(xml, ".png", ".ankitex");
	xml = replaceAllString(xml, ".jpg", ".ankitex");
	xml = replaceAllString(xml, ".jpeg", ".ankitex");

	// Write file
	File file;
	ANKI_CHECK(file.open(fname.toCString(), FileOpenFlag::WRITE));
	ANKI_CHECK(file.writeText("%s", xml.c_str()));

	return Error::NONE;
}

} // end namespace anki