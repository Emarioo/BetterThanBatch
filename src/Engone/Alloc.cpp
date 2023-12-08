#include "Engone/Alloc.h"

#include "Engone/PlatformLayer.h"
#include "BetBat/Config.h"

namespace engone {

	// bool Memory::resize(u64 count){
	// 	#ifdef DEBUG_RESIZE
	// 	printf("## Resize %lld -> %lld\n",max*getTypeSize(),count*getTypeSize());
	// 	#endif
	// 	if(getTypeSize()==0) return false;
	// 	if (count == 0) {
	// 		if (data) {
	// 			engone::SetTracker(false);
	// 			engone::Free(data, max * getTypeSize());
	// 			engone::SetTracker(true);
	// 		}
	// 		data = nullptr;
	// 		max = 0;
	// 		used = 0;
	// 	}else if (!data) {
	// 		engone::SetTracker(false);
	// 		data = engone::Allocate(count * getTypeSize());
	// 		engone::SetTracker(true);
	// 		if (data) {
	// 			max = count;
	// 			used = 0;
	// 		}
	// 	} else {
	// 		void* newData = 0;
	// 		engone::SetTracker(false);
	// 		newData = engone::Reallocate(data, max * getTypeSize(), count * getTypeSize());
	// 		engone::SetTracker(true);
	// 		if (newData) {
	// 			data = newData;
	// 			max = count;
	// 			if (max < used)
	// 				used = max;
	// 		}
	// 	}
	// 	return max == count; // returns true when intention was successful
	// }
}