#pragma once

#include <stdlib.h>

#include "Engone/PlatformLayer.h"

// #define ALLOC_NEW(CLASS) new((CLASS*)engone::Allocate(sizeof(CLASS))) CLASS
// #define ALLOC_DELETE(CLASS,VAR) {VAR->~CLASS();engone::Free(VAR,sizeof(CLASS));}

// #define ALLOC_TYPE_HEAP 0
// #define ALLOC_TYPE_GAME_MEMORY 1

namespace engone {
	template <typename T>
	struct Memory {
		// Memory(uint32 typeSize) : getTypeSize()(typeSize) {}
		Memory() {}

		T* data = nullptr;
		u64 max = 0;
		u64 used = 0; // may be useful to you.

		// count is not in bytes.
		// Rename to reserve?
		// will only return false if typeSize is 0
		bool resize(u64 count){
			#ifdef DEBUG_RESIZE
			printf("## Resize %lld -> %lld\n",max*getTypeSize(),count*getTypeSize());
			#endif
			if(getTypeSize()==0) return false;
			if (count == 0) {
				if (data) {
					engone::SetTracker(false);
					engone::Free(data, max * getTypeSize());
					engone::SetTracker(true);
				}
				data = nullptr;
				max = 0;
				used = 0;
			}else if (!data) {
				engone::SetTracker(false);
				data = (T*)engone::Allocate(count * getTypeSize());
				engone::SetTracker(true);
				if (data) {
					max = count;
					used = 0;
				}
			} else {
				T* newData = 0;
				engone::SetTracker(false);
				newData = (T*)engone::Reallocate(data, max * getTypeSize(), count * getTypeSize());
				engone::SetTracker(true);
				if (newData) {
					data = newData;
					max = count;
					if (max < used)
						used = max;
				}
			}
			return max == count; // returns true when intention was successful
		}
		u32 getTypeSize() const { return sizeof(T); }
		// read only, changing it will ruin things internally
		// uint32 m_typeSize = 0;
	};
}