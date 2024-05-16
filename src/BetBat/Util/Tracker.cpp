#ifdef gone
#include "BetBat/Util/Tracker.h"

#include "BetBat/Util/Utility.h"
#include "Engone/Util/Array.h"
#include "Engone/PlatformLayer.h"

struct TrackLocation {
    const char* file;
    int line, column;
    int alloc_id;
};
struct TrackerType {
    std::string name{};
    u32 size = 0;
    u32 count = 0;
    std::unordered_map<void*, TrackLocation> locations;
    // DynamicArray<DebugLocation> locations{}; // add tracking
    // DynamicArray<DebugLocation> freed_locations{}; // del tracking (not sure how reallocations are dealt with)
};
struct Tracker_impl {
    std::unordered_map<size_t, TrackerType> m_trackedTypes{}; // maps typeinfo hashcodes to tracker types
    engone::Mutex m_trackLock{};
    bool m_enableTracking = true;
    // bool m_enableTracking = false;

    volatile int allocation_id = 1;
};
static Tracker_impl* global_tracker = nullptr;

#define ENSURE_GLOBAL_TRACKER if(!global_tracker) { global_tracker = new Tracker_impl(); Assert(global_tracker); }

void EnableTracking(bool enabled) {
    ENSURE_GLOBAL_TRACKER
    global_tracker->m_trackLock.lock();
    global_tracker->m_enableTracking = enabled;
    global_tracker->m_trackLock.unlock();
}
void AddTracking(void* ptr, const std::type_info& typeInfo, u32 size, const DebugLocation& loc, u32 count) {
    ENSURE_GLOBAL_TRACKER
    if(!global_tracker->m_enableTracking)
        return;

    auto id = typeInfo.hash_code();

    int alloc_id = engone::atomic_add(&global_tracker->allocation_id, 1) - 1;

    // @DEBUG
    int alloc_ids[]{
        0,
        // 1, 103, 104, 106, 107
    };
    for(int i=0;i<sizeof(alloc_ids)/sizeof(*alloc_ids);i++) {
        BREAK(alloc_id == alloc_ids[i]);
    }

    global_tracker->m_trackLock.lock();

    bool prev = global_tracker->m_enableTracking;
    global_tracker->m_enableTracking = false;
    
    auto pair = global_tracker->m_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == global_tracker->m_trackedTypes.end()) {
        ptr = &(global_tracker->m_trackedTypes[id] = {});
        auto name = typeInfo.name();
        ptr->name = name;
        ptr->size = size;
    } else {
        ptr = &pair->second;
    }
    ptr->count+=count;
    #ifdef LOG_TRACKER
    engone::log::out << engone::log::GREEN << "Track "<<count<<": "<<typeInfo.name() <<" (left: "<<ptr->count<<")\n";
    #endif
    ptr->locations[]
    if(loc.file){
        ptr->locations.add(loc);
        ptr->locations.last().count = count;
        ptr->locations.last().alloc_id = alloc_id;
    }
    global_tracker->m_enableTracking = prev;
    global_tracker->m_trackLock.unlock();
}
void DelTracking(void* ptr, const std::type_info& typeInfo, u32 size, const DebugLocation& loc, u32 count) {
    ENSURE_GLOBAL_TRACKER
     if(!global_tracker->m_enableTracking)
        return;

    auto id = typeInfo.hash_code();

    global_tracker->m_trackLock.lock();

    int alloc_id = engone::atomic_add(&global_tracker->allocation_id, 1) - 1;

    bool prev = global_tracker->m_enableTracking;
    global_tracker->m_enableTracking = false;

    auto pair = global_tracker->m_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == global_tracker->m_trackedTypes.end()) {
        ptr = &(global_tracker->m_trackedTypes[id] = {});
        auto name = typeInfo.name();
        ptr->name = name;
        ptr->size = size;
    } else {
        ptr = &pair->second;
    }
    ptr->count-=count;
    #ifdef LOG_TRACKER
    engone::log::out << engone::log::CYAN << "Unrack "<<count<<": "<<typeInfo.name() <<" (left: "<<ptr->count<<")\n";
    #endif
    if(loc.file) {
        ptr->freed_locations.add(loc);
        ptr->freed_locations.last().count = count;
        ptr->freed_locations.last().alloc_id = alloc_id;

    //     bool removed = false;
    //     for(int i=0;i<ptr->locations.size();i++){
    //         TrackLocation& it = ptr->locations[i];
    //         if(it.count == count) {
    //             if(i<ptr->locations.size()-1) {
    //                 ptr->locations[i] = ptr->locations.last();
    //             }
    //             ptr->locations.pop();
    //             removed = true;
    //             break;
    //         }
    //     }
    //     // Assert(removed);
    }
    global_tracker->m_enableTracking = prev;
    global_tracker->m_trackLock.unlock();
}
void PrintTrackedTypes(){
    ENSURE_GLOBAL_TRACKER
    
    using namespace engone;

    global_tracker->m_trackLock.lock();
    bool prev = global_tracker->m_enableTracking;
    global_tracker->m_enableTracking = false;

    u32 totalMemory = 0;
    log::out << log::BLUE<<"Tracked types:\n";
    DynamicArray<DebugLocation> m_uniqueTrackLocations{};
    
    for(auto& pair : global_tracker->m_trackedTypes) {
        TrackerType* trackType = &pair.second;
        if(trackType->count == 0)
            continue;
        
        if(trackType->name.find("struct ") == 0) // Naming on Windows?
            log::out <<" "<<log::LIME<< (trackType->name.c_str()+strlen("struct "))<<log::NO_COLOR <<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
        else
            log::out <<" "<<log::LIME<< trackType->name <<log::NO_COLOR<<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
        m_uniqueTrackLocations.resize(0);
        for(int j=0;j<trackType->locations.size();j++){
            auto& loc = trackType->locations[j];
            bool found = false;
            for(int k=0;k<m_uniqueTrackLocations.size();k++){
                auto& loc2 = trackType->locations[k];
                if(loc.file == loc2.file && loc.line == loc2.line && loc.alloc_id == loc2.alloc_id) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                m_uniqueTrackLocations.add(loc);
            }
        }
        for(int k=0;k<m_uniqueTrackLocations.size();k++){
            auto& loc2 = trackType->locations[k];
            int len = strlen(loc2.file);
            int cutoff = len-1;
            int slashCount = 2;
            std::string path = loc2.file;
            
            ReplaceChar((char*)path.data(), path.length(),'\\','/');
            path = BriefString(path, 50);
            log::out << "  "<<(path) << ":"<<loc2.line<< ", id: "<<loc2.alloc_id<<"\n";
            // while(cutoff>0){
            //     if(loc2.fname[cutoff] == '/' || loc2.fname[cutoff] == '\\'){
            //         slashCount--;
            //         if(!slashCount) {
            //             cutoff++;
            //             break;
            //         }
            //     }
            //     cutoff--;
            // }
            // if(len > 25)
                // log::out << "  "<<(loc2.fname+(cutoff)) << ":"<<loc2.line<<"\n";
            // else
            //     log::out << "  "<<loc2.fname << ":"<<loc2.line<<"\n";
        }
        totalMemory += trackType->count * trackType->size;
    }
    if(totalMemory != 0)
        log::out << log::RED;
    log::out << "Tracked memory: "<<totalMemory<<"\n";
    
    global_tracker->m_enableTracking = prev;
    global_tracker->m_trackLock.unlock();
}
u32 GetMemoryUsage(){
    if(!global_tracker)
        return 0;
    u32 memory = 0;
    global_tracker->m_trackLock.lock();
    for(auto& pair : global_tracker->m_trackedTypes) {
        TrackerType* trackType = &pair.second;
        memory += trackType->locations.max * sizeof(DebugLocation);
    }
    global_tracker->m_trackLock.unlock();
    return memory;
}
#endif