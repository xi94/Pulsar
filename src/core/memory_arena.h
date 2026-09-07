#pragma once

/// A bump allocator over one up-front reservation. Reset rewinds the whole arena at once;
/// there is no individual free, which is exactly what per-frame scratch wants.
class CMemoryArena {
  public:
	CMemoryArena() = default;
	~CMemoryArena();

	CMemoryArena(const CMemoryArena &) = delete;
	CMemoryArena &operator=(const CMemoryArena &) = delete;

	bool Init(u64 capacity);
	void Reset()
	{
		m_nOffset = 0;
	}

	void *Alloc(u64 size, u64 alignment = alignof(std::max_align_t));

	template <typename T>
	T *AllocArray(u64 count)
	{
		return static_cast<T *>(Alloc(sizeof(T) * count, alignof(T)));
	}

  private:
	u8 *m_pBase = nullptr;
	u64 m_nCapacity = 0;
	u64 m_nOffset = 0;
};
