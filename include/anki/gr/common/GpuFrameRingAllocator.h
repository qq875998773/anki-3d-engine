// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/Common.h>

namespace anki
{

/// @addtogroup graphics
/// @{

/// Manages pre-allocated GPU memory for per frame usage.
class GpuFrameRingAllocator : public NonCopyable
{
	friend class DynamicMemorySerializeCommand;

public:
	GpuFrameRingAllocator()
	{
	}

	~GpuFrameRingAllocator()
	{
	}

	/// Initialize with pre-allocated always mapped memory.
	/// @param size The size of the GPU buffer.
	/// @param alignment The working alignment.
	/// @param maxAllocationSize The size in @a allocate cannot exceed
	///        maxAllocationSize.
	void init(
		PtrSize size, U32 alignment, PtrSize maxAllocationSize = MAX_PTR_SIZE);

	/// Allocate memory for a dynamic buffer.
	ANKI_USE_RESULT Error allocate(
		PtrSize size, DynamicBufferToken& token, Bool handleOomErrors = true);

	/// Call this at the end of the frame.
	/// @return The bytes that were not used. Used for statistics.
	PtrSize endFrame();

private:
	PtrSize m_size = 0; ///< The full size of the buffer.
	U32 m_alignment = 0; ///< Always work in that alignment.
	PtrSize m_maxAllocationSize = 0; ///< For debugging.

	Atomic<PtrSize> m_offset = {0};
	U64 m_frame = 0;

	Bool isCreated() const
	{
		return m_size > 0;
	}
};
/// @}

} // end namespace anki