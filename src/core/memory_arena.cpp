#include "core/memory_arena.h"

#include <cassert>
#include <cstdlib>

CMemoryArena::~CMemoryArena()
{
	if (m_pBase != nullptr) {
		std::free(m_pBase);
	}
}

bool CMemoryArena::Init(u64 capacity)
{
	assert(m_pBase == nullptr && "arena initialized twice");

	m_pBase = static_cast<u8 *>(std::malloc(capacity));
	if (m_pBase == nullptr) return false;

	m_nCapacity = capacity;
	m_nOffset = 0;

	return true;
}

void *CMemoryArena::Alloc(u64 size, u64 alignment)
{
	const u64 alignedOffset = (m_nOffset + alignment - 1) & ~(alignment - 1);
	const u64 newOffset = alignedOffset + size;

	assert(newOffset <= m_nCapacity && "arena exhausted - the call site that sized it got it wrong");

	m_nOffset = newOffset;

	return m_pBase + alignedOffset;
}
