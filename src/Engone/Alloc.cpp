#include "Engone/Alloc.h"

#include "Engone/PlatformLayer.h"
#include "BetBat/Config.h"

namespace engone {
	bool Memory::resize(uint64 count){
		#ifdef DEBUG_RESIZE
		printf("## Resize %lld -> %lld\n",max*m_typeSize,count*m_typeSize);
		#endif
		if(m_typeSize==0) return false;
		if (count == 0) {
			if (data) {
				engone::Free(data, max * m_typeSize);
			}
			data = nullptr;
			max = 0;
			used = 0;
		}else if (!data) {
			data = engone::Allocate(count * m_typeSize);
			if (data) {
				max = count;
				used = 0;
			}
		} else {
			void* newData = 0;
			newData = engone::Reallocate(data, max * m_typeSize, count * m_typeSize);
			if (newData) {
				data = newData;
				max = count;
				if (max < used)
					used = max;
			}
		}
		return max == count; // returns true when intention was successful
	}
}