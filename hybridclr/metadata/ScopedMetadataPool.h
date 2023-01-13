#pragma once

#include "../CommonDef.h"

namespace hybridclr
{
namespace metadata
{

	class ScopedMetadataPool
	{
	private:
		char* const _pool;
		const uint32_t _capacity;
		uint32_t _useSize;

	public:
		ScopedMetadataPool(char* pool, uint32_t capacity) : _pool(pool), _capacity(capacity), _useSize(0) {}

		void* AllocBytes(uint32_t size)
		{
			size = (size + 7) & ~0x7;
			int32_t oldUsedSize = _useSize;
			_useSize += size;
			IL2CPP_ASSERT(_useSize <= _capacity);
			return _pool + oldUsedSize;
		}

		template<typename T>
		T* Alloc()
		{
			return (T*)AllocBytes(sizeof(T));
		}

		template<typename T>
		T* Calloc(uint32_t count)
		{
			return (T*)AllocBytes(sizeof(T) * count);
		}

		template<typename T>
		T* CallocZero(uint32_t count)
		{
			uint32_t totalSize = sizeof(T) * count;
			void* bytes = AllocBytes(totalSize);
			std::memset(bytes, 0, totalSize);
			return (T*)bytes;
		}
	};
}
}