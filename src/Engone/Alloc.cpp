#include "Engone/Alloc.h"

#include "Engone/PlatformLayer.h"

namespace engone {
	bool Memory::resize(uint64 count){
		// printf("## Resize %p %lld %lld\n",data,max*m_typeSize,count*m_typeSize);
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
		//printf("Resize max: %d count: %d\n", max,count);
		return max == count; // returns true when intention was successful
	}
}