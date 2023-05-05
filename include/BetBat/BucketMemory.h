#pragma once

#include "Engone/Typedefs.h"

// linear allocation for nows
struct BucketMemory {
    const u64 BUCKET_SIZE;
    struct Bucket {
        void* data;
        u64 used;
    };
    Bucket* buckets=0;
    u64 used=0;
    u64 max=0;

    void* alloc(u64 size);
    // implement free?
};