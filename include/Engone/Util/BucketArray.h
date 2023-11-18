#pragma once

#include "Engone/Logger.h"
// #include "Engone/Util/Tracker.h"
#include "Engone/PlatformLayer.h"

// Be vary: Constructor/destructor should be called but it has not been tested.
template<typename T>
struct BucketArray {
    struct Bucket {
        u8* m_data = nullptr;
        u32 m_amountOfUsedSpots = 0;
        u32 m_max = 0; // this is the same for every bucket, removing this field won't save memory. Bucket will still be 16 bytes
        void* getValue(u32 index, u32 elementSize, u32 valuesPerBucket) {
		//return (Value*)(memory.data+ vpf/8+index*sizeof(Value));
            return (void*)(m_data + valuesPerBucket + index * elementSize);
        }
        bool getBool(u32 index) {
            //u32 i = index / 8;
            //u32 j = index % 8;
            //char byte = memory.data[i];
            //char bit = byte&(1<<j);
		    return *(m_data + index);
        }
        void setBool(u32 index, bool yes) {
            //u32 i = index / 8;
            //u32 j = index % 8;
            //if (yes) {
            //	memory.data[i] = memory.data[i] | (1 << j);
            //} else {
            //	memory.data[i] = memory.data[i] & (~(1 << j));
            //}
            *(m_data + index) = yes;
        }
    };

    // constructor does nothing except remember the variable
    // valuesPerBucket is forced to be divisible by 8. (data alignment * bits as bools)
    BucketArray(u32 valuesPerBucket) : m_valuesPerBucket(valuesPerBucket) {
        u32 off = valuesPerBucket & 7;
		if (off != 0)
			m_valuesPerBucket = (m_valuesPerBucket & (~7)) + 8;
    }
    ~BucketArray() { cleanup(); }
    // Does not call free on the items.
    void cleanup() {
        for (int i = 0; i < m_buckets_max; i++) {
			Bucket& bucket = m_buckets[i];
			if (bucket.m_max != 0) {
				for (int j = 0; j < m_valuesPerBucket; j++) {
					bool yes = bucket.getBool(j);
					if (yes) {
						T* ptr = (T*)bucket.getValue(j, sizeof(T), m_valuesPerBucket);
						ptr->~T();
					}
				}
			}
            engone::Free(bucket.m_data, bucket.m_max);
		}
        engone::Free(m_buckets, m_buckets_max * sizeof(Bucket));
        m_buckets = nullptr;
        m_buckets_max = 0;
		m_valueCount = 0;
    }
    
    // @return Index where element is. -1 if something failed.
    // @param value A pointer to the value of the new element. Note that a memcpy occurs. Value can be nullptr for zero initialized data.
    // @param outElement A pointer to the newly added element. Optional.
    u32 add(T* value, T** outElement = nullptr) {
		if (m_valuesPerBucket == 0) {
			engone::log::out << engone::log::RED << __FUNCTION__  << " : valuesPerBucket is 0\n";
			return -1;
		}
		//if(m_valuesPerFrame !=8)
		//	engone::log::out << engone::log::Disable;

		//engone::log::out << "FA : ADD "<<m_valuesPerFrame<<"\n";
		// Find available frame
		int bucketIndex = -1;
		for (int i = 0; i < m_buckets_max; i++) {
			Bucket& bucket = m_buckets[i];
			//engone::log::out << "FA : " << frame.count << " != " << m_valuesPerFrame<< "\n";
			if (bucket.m_amountOfUsedSpots != m_valuesPerBucket) {
				bucketIndex = i;
				//engone::log::out << "FA : found frame "<<i<<" with "<< frame.count << " objects\n";
				break;
			}
		}
		// Create new frame if non found
		if (bucketIndex == -1) {
			int newMax = m_buckets_max * 1.5 + 1;
            Bucket* newBuckets = (Bucket*)engone::Reallocate(m_buckets, m_buckets_max * sizeof(Bucket), newMax * sizeof(Bucket));
			if (!newBuckets) {
				//engone::log::out >> engone::log::Disable;
				engone::log::out << engone::log::RED << __FUNCTION__ << " : failed resize buckets\n";
				return -1;
			}
            m_buckets = newBuckets;
            
			memset(m_buckets + m_buckets_max, 0, (newMax - m_buckets_max) * sizeof(Bucket));
			// for (int i = m_buckets_max; i < newMax; i++) {
                
			// 	*((Frame*)m_frames.data + i) = Frame(m_frames.getAllocType()); // char
			// }
			bucketIndex = m_buckets_max;
            m_buckets_max = newMax;
			//engone::log::out << "FA : Create frame " << frameIndex << "\n";
		}

		// Insert value into bucket
		Bucket& bucket = m_buckets[bucketIndex];
		if (bucket.m_max == 0) {
            u32 newSize = m_valuesPerBucket /* /8 */ + m_valuesPerBucket * sizeof(T);
            bucket.m_data = (u8*)engone::Allocate(newSize);
			if (!bucket.m_data) {
				engone::log::out << engone::log::RED << __FUNCTION__<<" : failed resize bucket\n";
				//engone::log::out >> engone::log::Disable; 
				return -1;
			}
            bucket.m_max = newSize;

            // Assert(newSize % alignof(T) == 8);

			//memset(bucket.m_data,0,m_valuesPerFrame/8);
			memset(bucket.m_data, 0, bucket.m_max);
			//engone::log::out << "FA : Reserve values\n";
		}

		// Find empty slot
		int valueIndex = -1;
		for (int i = 0; i < m_valuesPerBucket; i++) {
			bool yes = bucket.getBool(i);
			if (!yes) {
				valueIndex = i;
				//engone::log::out << "FA : found spot " << i << "\n";
				break;
			} else {
				//engone::log::out << "FA : checked spot " << i <<"\n";
			}
		}

		if (valueIndex == -1) {
			engone::log::out << engone::log::RED << __FUNCTION__<<" : Impossible error adding value in bucket " << bucketIndex << "\n";
			//engone::log::out >> engone::log::Disable;
			return -1;
		}
		bucket.m_amountOfUsedSpots++;
		//engone::log::out << "Frame : New object count " << frame.count << "\n";
		m_valueCount++;
		bucket.setBool(valueIndex, true);
		T* ptr = (T*)bucket.getValue(valueIndex, sizeof(T), m_valuesPerBucket);
		if (value)
			memcpy(ptr,value,sizeof(T));
		else
			new(ptr)T();
			// memset(ptr, 0, sizeof(T));
		//engone::log::out >> engone::log::Disable;
		if (outElement)
			*outElement = ptr;
		//engone::log::out << "FA:Add " << valueIndex << "\n";
		return bucketIndex * m_valuesPerBucket + valueIndex;
	}
	// @return Index where element is. -1 if something failed.
    // @param value A pointer to the value of the new element. Note that a memcpy occurs. Value can be nullptr for zero initialized data.
    // @param outElement A pointer to the newly added element. Optional.
    bool requestSpot(int requestedIndex, T* value, T** outElement = nullptr) {
		if (m_valuesPerBucket == 0) {
			engone::log::out << engone::log::RED << __FUNCTION__  << " : valuesPerBucket is 0\n";
			return false;
		}

		u32 bucketIndex = requestedIndex / m_valuesPerBucket;
		u32 valueIndex = requestedIndex % m_valuesPerBucket;

		if (bucketIndex >= m_buckets_max) {
			int newMax = m_buckets_max * 1.5 + 1;
            Bucket* newBuckets = (Bucket*)engone::Reallocate(m_buckets, m_buckets_max * sizeof(Bucket), newMax * sizeof(Bucket));
			if (!newBuckets) {
				//engone::log::out >> engone::log::Disable;
				engone::log::out << engone::log::RED << __FUNCTION__ << " : failed resize buckets\n";
				return false;
			}
            m_buckets = newBuckets;
            
			memset(m_buckets + m_buckets_max, 0, (newMax - m_buckets_max) * sizeof(Bucket));
			bucketIndex = m_buckets_max;
            m_buckets_max = newMax;
		}

		Bucket& bucket = m_buckets[bucketIndex];

		if (bucket.m_max == 0) {
			u32 newSize = m_valuesPerBucket /* /8 */ + m_valuesPerBucket * sizeof(T);
            bucket.m_data = (u8*)engone::Allocate(newSize);
			if (!bucket.m_data) {
				engone::log::out << engone::log::RED << __FUNCTION__<<" : failed resize bucket\n";
				return false;
			}
            bucket.m_max = newSize;

			//memset(bucket.m_data,0,m_valuesPerFrame/8);
			memset(bucket.m_data, 0, bucket.m_max);
		}

		bool yes = bucket.getBool(valueIndex);
		if (yes)
			return false;

		bucket.m_amountOfUsedSpots++;
		m_valueCount++;
		bucket.setBool(valueIndex,true);

		T* ptr = (T*)bucket.getValue(valueIndex, sizeof(T), m_valuesPerBucket);
		if (value)
			memcpy(ptr,value,sizeof(T));
		else
			new(ptr)T();
			// memset(ptr, 0, sizeof(T));
		//engone::log::out >> engone::log::Disable;
		if (outElement)
			*outElement = ptr;
		//engone::log::out << "FA:Add " << valueIndex << "\n";
		return true;
	}
    T* get(u32 index) {
		u32 bucketIndex = index / m_valuesPerBucket;
		u32 valueIndex = index % m_valuesPerBucket;

		if (bucketIndex >= m_buckets_max)
			return nullptr;

		Bucket& bucket = m_buckets[bucketIndex];

		if (valueIndex >= bucket.m_max)
			return nullptr;

		bool yes = bucket.getBool(valueIndex);
		if (!yes) // Check if slot is empty
			return nullptr;

		T* ptr = (T*)bucket.getValue(valueIndex, sizeof(T), m_valuesPerBucket);
		return ptr;
	}
    void removeAt(u32 index) {
		u32 bucketIndex = index / m_valuesPerBucket;
		u32 valueIndex = index % m_valuesPerBucket;

		if (bucketIndex >= m_buckets_max)
			return;

		Bucket& bucket = m_buckets[bucketIndex];

		if (bucket.m_max == 0)
			return;

		bool yes = bucket.getBool(valueIndex);
		if (!yes)
			return;

		bucket.m_amountOfUsedSpots--;
		m_valueCount--;
		T* ptr = (T*)bucket.getValue(valueIndex, sizeof(T), m_valuesPerBucket);
		ptr->~T();
		bucket.setBool(valueIndex,false);

		//if (frame.count==0) {
		//	frame.resize(0);
		//}
	}
    
    // in bytes
    u32 getMemoryUsage() {
		u32 bytes = m_buckets_max * sizeof(Bucket);
		for (int i = 0; i < m_buckets_max.max; i++) {
			bytes += m_buckets[i].m_max;
		}
		return bytes;
	}

    // YOU CANNOT USE THIS VALUE TO ITERATE THROUGH THE ELEMENTS!
    u32 getCount() {
        return m_valueCount;
    }

    struct Iterator {
        int index=-1;
        T* ptr=nullptr;
    };
    // Returns false if nothing more to iterate. Values in iterator are reset
    bool iterate(Iterator& iterator)  {
		while (true) {
            iterator.index++;
			u32 bucketIndex = iterator.index / m_valuesPerBucket;
			u32 valueIndex = iterator.index % m_valuesPerBucket;

			if (bucketIndex >= m_buckets_max)
				break;
			Bucket& bucket = m_buckets[bucketIndex];
			if (valueIndex >= bucket.m_max)
				break;

			bool yes = bucket.getBool(valueIndex);
			if (!yes)
				continue;

			iterator.ptr = (T*)bucket.getValue(valueIndex, sizeof(T), m_valuesPerBucket);
			return true;
		}
		iterator.index = -1;
		iterator.ptr = nullptr;
		return false;
	}

	// struct Serializer {
	// 	u32 progress = 0;
	// 	void* ptr = nullptr; // pointer to a part of data which should be serialized
	// 	u32 size = 0; // size of the data to serialize
	// };
	// // @param serializer Will be filled with a pointer and size which represents a part of the BucketArray that needs to be serialized.
	// // @return True when there is more data to serialize. False when you have serialized everything.
	// bool serialize(Serializer& serializer) {
	// 	if(serializer.progress == 0) {
			
	// 	} else {
	// 		m_buckets.
	// 		serializer.
	// 		m_buckets_max 
	// 	}
	// }
	// bool deserialize(void* ptr, u32 size) {
		
	// }

    // max = x/8+x*C
    // max = x*(1/8+C)
    // max/(1/8+C)
    // 8*max / (1+8*C)

    Bucket* m_buckets = nullptr;
    u32 m_buckets_max = 0;

    u32 m_valueCount = 0;
    u32 m_valuesPerBucket = 0;
};

void FrameArrayTest();

// void FrameArrayTest() {
// 	struct Apple {
// 		float x, y, size;
// 		void print() {
// 			printf("{%f, %f, %f}\n", x,y,size);
// 		}
// 	};
// 	FrameArray arr(sizeof(Apple),80,ALLOC_TYPE_HEAP);
// 	Apple tmp{ 25,15,92 };
// 	u32 id_a = arr.add(&tmp);
// 	tmp = { 25,15,92 };
// 	u32 id_b = arr.add(&tmp);
    
// 	arr.remove(id_a);//

// 	Apple* first = (Apple*)arr.get(0);

// 	printf("%p\n", first);
// 	tmp = { 92,61,22 };
// 	int id_c = arr.add(&tmp);

// 	//printf("%d\n", id_c);
// 	//a->print();
// 	//b->print();
    
// 	FrameArray::Iterator iterator;
// 	while (arr.iterate(iterator)) {
// 		((Apple*)iterator.ptr)->print();
// 	}


// 	std::cin.get();
// }