// Copyright (C) 2009-2017, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/vulkan/DescriptorObject.h>
#include <anki/gr/vulkan/GpuMemoryManager.h>
#include <anki/gr/common/Misc.h>
#include <anki/util/HashMap.h>

namespace anki
{

/// @addtogroup vulkan
/// @{

enum class TextureImplWorkaround : U8
{
	NONE,
	R8G8B8_TO_R8G8B8A8 = 1 << 0,
	S8_TO_D24S8 = 1 << 1,
	D24S8_TO_D32S8 = 1 << 2,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(TextureImplWorkaround, inline)

/// Texture container.
class TextureImpl : public DescriptorObject
{
public:
	SamplerPtr m_sampler;

	VkImage m_imageHandle = VK_NULL_HANDLE;

	GpuMemoryHandle m_memHandle;

	U32 m_width = 0;
	U32 m_height = 0;
	U32 m_depth = 0;
	TextureType m_type = TextureType::CUBE;
	U8 m_mipCount = 0;
	U32 m_layerCount = 0;
	VkImageAspectFlags m_aspect = 0;
	DepthStencilAspectBit m_akAspect = DepthStencilAspectBit::NONE;
	TextureUsageBit m_usage = TextureUsageBit::NONE;
	TextureUsageBit m_usageWhenEncountered = TextureUsageBit::NONE;
	PixelFormat m_format;
	VkFormat m_vkFormat = VK_FORMAT_UNDEFINED;

	Bool m_depthStencil = false;
	TextureImplWorkaround m_workarounds = TextureImplWorkaround::NONE;

	TextureImpl(GrManager* manager);

	~TextureImpl();

	ANKI_USE_RESULT Error init(const TextureInitInfo& init, Texture* tex);

	void checkSurface(const TextureSurfaceInfo& surf) const
	{
		checkTextureSurface(m_type, m_depth, m_mipCount, m_layerCount, surf);
	}

	void checkVolume(const TextureVolumeInfo& vol) const
	{
		ANKI_ASSERT(m_type == TextureType::_3D);
		ANKI_ASSERT(vol.m_level < m_mipCount);
	}

	void computeSubResourceRange(
		const TextureSurfaceInfo& surf, DepthStencilAspectBit aspect, VkImageSubresourceRange& range) const;

	void computeSubResourceRange(
		const TextureVolumeInfo& vol, DepthStencilAspectBit aspect, VkImageSubresourceRange& range) const;

	/// Compute the layer as defined by Vulkan.
	U computeVkArrayLayer(const TextureSurfaceInfo& surf) const;

	U computeVkArrayLayer(const TextureVolumeInfo& vol) const
	{
		return 0;
	}

	Bool usageValid(TextureUsageBit usage) const
	{
		return (usage & m_usage) == usage;
	}

	VkImageView getOrCreateSingleSurfaceView(const TextureSurfaceInfo& surf, DepthStencilAspectBit aspect);

	VkImageView getOrCreateSingleLevelView(U level, DepthStencilAspectBit aspect);

	/// That view will be used in descriptor sets.
	VkImageView getOrCreateResourceGroupView(DepthStencilAspectBit aspect);

	/// By knowing the previous and new texture usage calculate the relavant info for a ppline barrier.
	void computeBarrierInfo(TextureUsageBit before,
		TextureUsageBit after,
		U level,
		VkPipelineStageFlags& srcStages,
		VkAccessFlags& srcAccesses,
		VkPipelineStageFlags& dstStages,
		VkAccessFlags& dstAccesses) const;

	/// Predict the image layout.
	VkImageLayout computeLayout(TextureUsageBit usage, U level) const;

	VkImageAspectFlags convertAspect(DepthStencilAspectBit ak) const;

private:
	class ViewHasher
	{
	public:
		U64 operator()(const VkImageViewCreateInfo& b) const
		{
			return computeHash(&b, sizeof(b));
		}
	};

	class ViewCompare
	{
	public:
		Bool operator()(const VkImageViewCreateInfo& a, const VkImageViewCreateInfo& b) const
		{
			return memcmp(&a, &b, sizeof(a)) == 0;
		}
	};

	HashMap<VkImageViewCreateInfo, VkImageView, ViewHasher, ViewCompare> m_viewsMap;
	Mutex m_viewsMapMtx;
	VkImageViewCreateInfo m_viewCreateInfoTemplate;

	ANKI_USE_RESULT static VkFormatFeatureFlags calcFeatures(const TextureInitInfo& init);

	ANKI_USE_RESULT static VkImageCreateFlags calcCreateFlags(const TextureInitInfo& init);

	ANKI_USE_RESULT Bool imageSupported(const TextureInitInfo& init);

	ANKI_USE_RESULT Error initImage(const TextureInitInfo& init);

	VkImageView getOrCreateView(const VkImageViewCreateInfo& ci);
};
/// @}

} // end namespace anki

#include <anki/gr/vulkan/TextureImpl.inl.h>
