// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/vulkan/VulkanObject.h>

namespace anki
{

// Forward
class FramebufferAttachmentInfo;

/// @addtogroup vulkan
/// @{

/// Framebuffer implementation.
class FramebufferImpl : public VulkanObject
{
public:
	FramebufferImpl(GrManager* manager)
		: VulkanObject(manager)
	{
	}

	~FramebufferImpl();

	ANKI_USE_RESULT Error init(const FramebufferInitInfo& init);

	VkFramebuffer getFramebufferHandle(U idx) const
	{
		ANKI_ASSERT(m_framebuffers[idx]);
		return m_framebuffers[idx];
	}

	VkRenderPass getRenderPassHandle() const
	{
		ANKI_ASSERT(m_renderPass);
		return m_renderPass;
	}

	Bool isDefaultFramebuffer() const
	{
		return m_defaultFramebuffer;
	}

	U getAttachmentCount() const
	{
		ANKI_ASSERT(m_attachmentCount > 0);
		return m_attachmentCount;
	}

	const VkClearValue* getClearValues() const
	{
		return &m_clearVals[0];
	}

private:
	Array<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> m_framebuffers = {{
		0,
	}};
	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	Array<VkClearValue, MAX_COLOR_ATTACHMENTS + 1> m_clearVals;
	Bool8 m_defaultFramebuffer = false;
	U8 m_attachmentCount = 0;

	ANKI_USE_RESULT Error initRenderPass(const FramebufferInitInfo& init);

	void setupAttachmentDescriptor(const FramebufferAttachmentInfo& in,
		VkAttachmentDescription& out,
		Bool depthStencil);

	ANKI_USE_RESULT Error initFramebuffer(const FramebufferInitInfo& init);
};
/// @}

} // end namespace anki
