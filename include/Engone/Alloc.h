#pragma once

#include <stdlib.h>

#include "Engone/PlatformLayer.h"

#define ALLOC_NEW(CLASS) new((CLASS*)engone::Allocate(sizeof(CLASS))) CLASS
#define ALLOC_DELETE(CLASS,VAR) {VAR->~CLASS();engone::Free(VAR,sizeof(CLASS));}

#define ALLOC_TYPE_HEAP 0
#define ALLOC_TYPE_GAME_MEMORY 1

namespace engone {
	struct Memory {
		Memory(uint32 typeSize) : m_typeSize(typeSize) {}

		uint64 max = 0;
		uint64 used = 0; // may be useful to you.
		void* data = nullptr;

		// count is not in bytes.
		// Rename to reserve?
		bool resize(uint64 count);
		
		// read only, changing it will ruin things internally
		uint32 m_typeSize = 0;
	};
}