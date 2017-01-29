// Copyright (C) 2009-2017, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/vulkan/DescriptorSet.h>
#include <anki/gr/Buffer.h>
#include <anki/gr/vulkan/BufferImpl.h>
#include <anki/util/List.h>
#include <anki/util/HashMap.h>
#include <algorithm>

namespace anki
{

/// Descriptor set internal class.
class DS : public IntrusiveListEnabled<DS>, public IntrusiveHashMapEnabled<DS>
{
public:
	VkDescriptorSet m_handle = {};
	U64 m_lastFrameUsed = MAX_U64;
};

/// Per thread allocator.
class DSThreadAllocator : public NonCopyable
{
public:
	const DSLayoutCacheEntry* m_layoutEntry; ///< Know your father.

	ThreadId m_tid;
	DynamicArray<VkDescriptorPool> m_pools;
	U32 m_lastPoolDSCount = 0;
	U32 m_lastPoolFreeDSCount = 0;

	IntrusiveList<DS> m_list; ///< At the left of the list are the least used sets.
	IntrusiveHashMap<U64, DS> m_hashmap;

	DSThreadAllocator(const DSLayoutCacheEntry* layout, ThreadId tid)
		: m_layoutEntry(layout)
		, m_tid(tid)
	{
		ANKI_ASSERT(m_layoutEntry);
	}

	~DSThreadAllocator();

	ANKI_USE_RESULT Error init();
	ANKI_USE_RESULT Error createNewPool();

	ANKI_USE_RESULT Error getOrCreateSet(
		U64 hash, const Array<AnyBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET>& bindings, const DS*& out)
	{
		out = tryFindSet(hash);
		if(out == nullptr)
		{
			ANKI_CHECK(newSet(hash, bindings, out));
		}

		return ErrorCode::NONE;
	}

private:
	ANKI_USE_RESULT const DS* tryFindSet(U64 hash);
	ANKI_USE_RESULT Error newSet(
		U64 hash, const Array<AnyBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET>& bindings, const DS*& out);
	void writeSet(const Array<AnyBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET>& bindings, const DS& set);
};

/// Cache entry. It's built around a specific descriptor set layout.
class DSLayoutCacheEntry
{
public:
	DescriptorSetFactory* m_factory;

	U64 m_hash = 0; ///< Layout hash.
	VkDescriptorSetLayout m_layoutHandle = {};
	BitSet<MAX_BINDINGS_PER_DESCRIPTOR_SET, U8> m_activeBindings = {false};
	U32 m_minBinding = MAX_U32;
	U32 m_maxBinding = 0;

	// Cache the create info
	Array<VkDescriptorPoolSize, U(DescriptorType::COUNT)> m_poolSizesCreateInf = {};
	VkDescriptorPoolCreateInfo m_poolCreateInf = {};

	DynamicArray<DSThreadAllocator*> m_threadAllocs;
	SpinLock m_threadAllocsMtx;

	DSLayoutCacheEntry(DescriptorSetFactory* factory)
		: m_factory(factory)
	{
	}

	~DSLayoutCacheEntry();

	ANKI_USE_RESULT Error init(const DescriptorBinding* bindings, U bindingCount, U64 hash);

	ANKI_USE_RESULT Error getOrCreateThreadAllocator(ThreadId tid, DSThreadAllocator*& alloc);
};

DSThreadAllocator::~DSThreadAllocator()
{
	auto alloc = m_layoutEntry->m_factory->m_alloc;

	while(!m_list.isEmpty())
	{
		DS* ds = &m_list.getFront();
		m_list.popFront();

		alloc.deleteInstance(ds);
	}

	for(VkDescriptorPool pool : m_pools)
	{
		vkDestroyDescriptorPool(m_layoutEntry->m_factory->m_dev, pool, nullptr);
	}
	m_pools.destroy(alloc);
}

Error DSThreadAllocator::init()
{
	ANKI_CHECK(createNewPool());
	return ErrorCode::NONE;
}

Error DSThreadAllocator::createNewPool()
{
	m_lastPoolDSCount =
		(m_lastPoolDSCount != 0) ? (m_lastPoolDSCount * DESCRIPTOR_POOL_SIZE_SCALE) : DESCRIPTOR_POOL_INITIAL_SIZE;
	m_lastPoolFreeDSCount = m_lastPoolDSCount;

	// Set the create info
	Array<VkDescriptorPoolSize, U(DescriptorType::COUNT)> poolSizes;
	memcpy(&poolSizes[0],
		&m_layoutEntry->m_poolSizesCreateInf[0],
		sizeof(poolSizes[0]) * m_layoutEntry->m_poolCreateInf.poolSizeCount);

	for(U i = 0; i < m_layoutEntry->m_poolCreateInf.poolSizeCount; ++i)
	{
		poolSizes[i].descriptorCount *= m_lastPoolDSCount;
		ANKI_ASSERT(poolSizes[i].descriptorCount > 0);
	}

	VkDescriptorPoolCreateInfo ci = m_layoutEntry->m_poolCreateInf;
	ci.pPoolSizes = &poolSizes[0];
	ci.maxSets = m_lastPoolDSCount;

	// Create
	VkDescriptorPool pool;
	ANKI_VK_CHECK(vkCreateDescriptorPool(m_layoutEntry->m_factory->m_dev, &ci, nullptr, &pool));

	// Push back
	m_pools.resize(m_layoutEntry->m_factory->m_alloc, m_pools.getSize() + 1);
	m_pools[m_pools.getSize() - 1] = pool;

	return ErrorCode::NONE;
}

const DS* DSThreadAllocator::tryFindSet(U64 hash)
{
	ANKI_ASSERT(hash > 0);

	auto it = m_hashmap.find(hash);
	if(it != m_hashmap.getEnd())
	{
		return nullptr;
	}
	else
	{
		DS* ds = &(*it);

		// Remove from the list and place at the end of the list
		m_list.erase(ds);
		m_list.pushBack(ds);
		ds->m_lastFrameUsed = m_layoutEntry->m_factory->m_frameCount;

		return ds;
	}
}

Error DSThreadAllocator::newSet(
	U64 hash, const Array<AnyBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET>& bindings, const DS*& out_)
{
	DS* out = nullptr;

	// First try to see if there are unused to recycle
	const U64 crntFrame = m_layoutEntry->m_factory->m_frameCount;
	auto it = m_list.getBegin();
	const auto end = m_list.getEnd();
	while(it != end)
	{
		DS* set = &(*it);
		U64 frameDiff = crntFrame - set->m_lastFrameUsed;
		if(frameDiff > DESCRIPTOR_FRAME_BUFFERING)
		{
			// Found something, recycle
			m_hashmap.erase(set);
			m_list.erase(set);
			m_list.pushBack(set);
			m_hashmap.pushBack(hash, set);

			out = set;
			break;
		}
	}

	if(out == nullptr)
	{
		// Need to allocate one

		if(m_lastPoolFreeDSCount == 0)
		{
			// Can't allocate one from the current pool, create new
			ANKI_CHECK(createNewPool());
		}

		VkDescriptorSetAllocateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ci.descriptorPool = m_pools.getBack();
		ci.pSetLayouts = &m_layoutEntry->m_layoutHandle;
		ci.descriptorSetCount = 1;

		VkDescriptorSet handle;
		VkResult rez = vkAllocateDescriptorSets(m_layoutEntry->m_factory->m_dev, &ci, &handle);
		(void)rez;
		ANKI_ASSERT(rez == VK_SUCCESS && "That allocation can't fail");

		out = m_layoutEntry->m_factory->m_alloc.newInstance<DS>();
		out->m_handle = handle;
	}

	ANKI_ASSERT(out);
	out->m_lastFrameUsed = m_layoutEntry->m_factory->m_frameCount;

	// Finally, write it
	writeSet(bindings, *out);

	out_ = out;
	return ErrorCode::NONE;
}

void DSThreadAllocator::writeSet(const Array<AnyBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET>& bindings, const DS& set)
{
	Array<VkWriteDescriptorSet, MAX_BINDINGS_PER_DESCRIPTOR_SET> writes;
	U writeCount = 0;

	Array<VkDescriptorImageInfo, MAX_TEXTURE_BINDINGS> tex;
	U texCount = 0;
	Array<VkDescriptorBufferInfo, MAX_UNIFORM_BUFFER_BINDINGS + MAX_STORAGE_BUFFER_BINDINGS> buff;
	U buffCount = 0;

	VkWriteDescriptorSet writeTemplate = {};
	writeTemplate.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTemplate.pNext = nullptr;
	writeTemplate.dstSet = set.m_handle;
	writeTemplate.descriptorCount = 1;

	for(U i = m_layoutEntry->m_minBinding; i <= m_layoutEntry->m_maxBinding; ++i)
	{
		if(m_layoutEntry->m_activeBindings.get(i))
		{
			const AnyBinding& b = bindings[i];

			VkWriteDescriptorSet& w = writes[writeCount++];
			w = writeTemplate;
			w.dstBinding = i;
			w.descriptorType = convertDescriptorType(b.m_type);

			switch(b.m_type)
			{
			case DescriptorType::TEXTURE:
				tex[texCount].sampler = b.m_tex.m_sampler->m_handle;
				tex[texCount].imageView = b.m_tex.m_tex->getOrCreateResourceGroupView(b.m_tex.m_aspect);
				tex[texCount].imageLayout = b.m_tex.m_layout;

				w.pImageInfo = &tex[texCount];

				++texCount;
				break;
			case DescriptorType::UNIFORM_BUFFER:
			case DescriptorType::STORAGE_BUFFER:
				buff[buffCount].buffer = b.m_buff.m_buff->getHandle();
				buff[buffCount].offset = b.m_buff.m_offset;
				buff[buffCount].range = b.m_buff.m_range;

				w.pBufferInfo = &buff[buffCount];

				++buffCount;
				break;
			case DescriptorType::IMAGE:
				tex[texCount].sampler = VK_NULL_HANDLE;
				tex[texCount].imageView = b.m_image.m_tex->getOrCreateSingleSurfaceView(
					TextureSurfaceInfo(b.m_image.m_level, 0, 0, 0), b.m_tex.m_aspect);
				tex[texCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				w.pImageInfo = &tex[texCount];

				++texCount;
				break;
			default:
				ANKI_ASSERT(0);
			}
		}
	}

	vkUpdateDescriptorSets(m_layoutEntry->m_factory->m_dev, writeCount, &writes[0], 0, nullptr);
}

DSLayoutCacheEntry::~DSLayoutCacheEntry()
{
	auto alloc = m_factory->m_alloc;

	for(DSThreadAllocator* a : m_threadAllocs)
	{
		alloc.deleteInstance(a);
	}

	m_threadAllocs.destroy(alloc);

	if(m_layoutHandle)
	{
		vkDestroyDescriptorSetLayout(m_factory->m_dev, m_layoutHandle, nullptr);
	}
}

Error DSLayoutCacheEntry::init(const DescriptorBinding* bindings, U bindingCount, U64 hash)
{
	ANKI_ASSERT(bindings);
	ANKI_ASSERT(hash > 0);

	m_hash = hash;

	// Create the VK layout
	Array<VkDescriptorSetLayoutBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET> vkBindings;
	VkDescriptorSetLayoutCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	for(U i = 0; i < bindingCount; ++i)
	{
		VkDescriptorSetLayoutBinding& vk = vkBindings[i];
		const DescriptorBinding& ak = bindings[i];

		vk.binding = ak.m_binding;
		vk.descriptorCount = 1;
		vk.descriptorType = convertDescriptorType(ak.m_type);
		vk.pImmutableSamplers = nullptr;
		vk.stageFlags = convertShaderTypeBit(ak.m_stageMask);

		ANKI_ASSERT(m_activeBindings.get(ak.m_binding) == false);
		m_activeBindings.set(ak.m_binding);
		m_minBinding = min<U32>(m_minBinding, ak.m_binding);
		m_maxBinding = max<U32>(m_maxBinding, ak.m_binding);
	}

	ci.bindingCount = bindingCount;
	ci.pBindings = &vkBindings[0];

	ANKI_VK_CHECK(vkCreateDescriptorSetLayout(m_factory->m_dev, &ci, nullptr, &m_layoutHandle));

	// Create the pool info
	U poolSizeCount = 0;
	for(U i = 0; i < bindingCount; ++i)
	{
		U j;
		for(j = 0; j < poolSizeCount; ++j)
		{
			if(m_poolSizesCreateInf[j].type == convertDescriptorType(bindings[i].m_type))
			{
				++m_poolSizesCreateInf[j].descriptorCount;
				break;
			}
		}

		if(j == poolSizeCount)
		{
			m_poolSizesCreateInf[poolSizeCount].type = convertDescriptorType(bindings[i].m_type);

			switch(m_poolSizesCreateInf[poolSizeCount].type)
			{
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				m_poolSizesCreateInf[poolSizeCount].descriptorCount = MAX_TEXTURE_BINDINGS;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				m_poolSizesCreateInf[poolSizeCount].descriptorCount = MAX_UNIFORM_BUFFER_BINDINGS;
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				m_poolSizesCreateInf[poolSizeCount].descriptorCount = MAX_STORAGE_BUFFER_BINDINGS;
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				m_poolSizesCreateInf[poolSizeCount].descriptorCount = MAX_IMAGE_BINDINGS;
				break;
			default:
				ANKI_ASSERT(0);
			}

			m_poolSizesCreateInf[poolSizeCount].descriptorCount = 1;
			++poolSizeCount;
		}
	}

	ANKI_ASSERT(poolSizeCount > 0);

	m_poolCreateInf.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	m_poolCreateInf.poolSizeCount = poolSizeCount;

	return ErrorCode::NONE;
}

Error DSLayoutCacheEntry::getOrCreateThreadAllocator(ThreadId tid, DSThreadAllocator*& alloc)
{
	alloc = nullptr;

	LockGuard<SpinLock> lock(m_threadAllocsMtx);

	class Comp
	{
	public:
		Bool operator()(const DSThreadAllocator* a, ThreadId tid) const
		{
			return a->m_tid < tid;
		}

		Bool operator()(ThreadId tid, const DSThreadAllocator* a) const
		{
			return tid < a->m_tid;
		}
	};

	// Find using binary search
	auto it = std::lower_bound(m_threadAllocs.getBegin(), m_threadAllocs.getEnd(), tid, Comp());

	if(it != m_threadAllocs.getEnd())
	{
		alloc = *it;
	}
	else
	{
		// Need to create one

		alloc = m_factory->m_alloc.newInstance<DSThreadAllocator>(this, tid);
		ANKI_CHECK(alloc->init());

		m_threadAllocs.resize(m_factory->m_alloc, m_threadAllocs.getSize() + 1);
		m_threadAllocs[m_threadAllocs.getSize() - 1] = alloc;

		// Sort for fast find
		std::sort(m_threadAllocs.getBegin(),
			m_threadAllocs.getEnd(),
			[](const DSThreadAllocator* a, const DSThreadAllocator* b) { return a->m_tid < b->m_tid; });
	}

	ANKI_ASSERT(alloc);
	return ErrorCode::NONE;
}

DescriptorSetFactory::~DescriptorSetFactory()
{
}

void DescriptorSetFactory::init(const GrAllocator<U8>& alloc, VkDevice dev)
{
	m_alloc = alloc;
	m_dev = dev;
}

void DescriptorSetFactory::destroy()
{
	for(DSLayoutCacheEntry* l : m_caches)
	{
		m_alloc.deleteInstance(l);
	}

	m_caches.destroy(m_alloc);
}

Error DescriptorSetFactory::newDescriptorSetLayout(const DescriptorSetLayoutInitInfo& init, DescriptorSetLayout& layout)
{
	// Compute the hash for the layout
	Array<DescriptorBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET> bindings;
	U bindingCount = init.m_bindings.getSize();
	U64 hash;

	if(init.m_bindings.getSize() > 0)
	{
		memcpy(&bindings[0], &init.m_bindings[0], init.m_bindings.getSizeInBytes());
		std::sort(&bindings[0],
			&bindings[0] + bindingCount,
			[](const DescriptorBinding& a, const DescriptorBinding& b) { return a.m_binding < b.m_binding; });

		hash = computeHash(&bindings[0], init.m_bindings.getSizeInBytes());
		ANKI_ASSERT(hash != 1);
	}
	else
	{
		hash = 1;
	}

	// Find or create the cache entry
	LockGuard<SpinLock> lock(m_cachesMtx);

	DSLayoutCacheEntry* cache = nullptr;
	U cacheIdx = MAX_U;
	U count = 0;
	for(DSLayoutCacheEntry* it : m_caches)
	{
		if(it->m_hash == hash)
		{
			cache = it;
			cacheIdx = count;
			break;
		}
		++count;
	}

	if(cache == nullptr)
	{
		cache = m_alloc.newInstance<DSLayoutCacheEntry>(this);
		ANKI_CHECK(cache->init(&bindings[0], bindingCount, hash));

		m_caches.resize(m_alloc, m_caches.getSize() + 1);
		m_caches[m_caches.getSize() - 1] = cache;
		cacheIdx = m_caches.getSize() - 1;
	}

	// Set the layout
	layout.m_handle = cache->m_layoutHandle;
	layout.m_cacheEntryIdx = cacheIdx;

	return ErrorCode::NONE;
}

Error DescriptorSetFactory::newDescriptorSet(ThreadId tid, const DescriptorSetState& init, DescriptorSet& set)
{
	// Get cache entry
	m_cachesMtx.lock();
	DSLayoutCacheEntry& entry = *m_caches[init.m_layout.m_cacheEntryIdx];
	m_cachesMtx.unlock();

	// Get thread allocator
	DSThreadAllocator* alloc;
	ANKI_CHECK(entry.getOrCreateThreadAllocator(tid, alloc));

	// Compute the hash
	Array<U64, MAX_BINDINGS_PER_DESCRIPTOR_SET> uuids;
	U uuidCount = 0;

	const U minBinding = entry.m_minBinding;
	const U maxBinding = entry.m_maxBinding;
	for(U i = minBinding; i <= maxBinding; ++i)
	{
		if(entry.m_activeBindings.get(i))
		{
			uuids[uuidCount++] = init.m_bindings[i].m_uuids[0];
			uuids[uuidCount++] = init.m_bindings[i].m_uuids[1];
		}
	}

	U64 hash = computeHash(&uuids[0], uuidCount * sizeof(U64));

	// Finally, allocate
	const DS* s;
	ANKI_CHECK(alloc->getOrCreateSet(hash, init.m_bindings, s));
	set.m_handle = s->m_handle;
	ANKI_ASSERT(set.m_handle != VK_NULL_HANDLE);

	return ErrorCode::NONE;
}

} // end namespace anki
