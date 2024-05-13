#include "BetBat/Util/Tracker.h"

#include "BetBat/Util/Utility.h"
#include "Engone/Util/Array.h"
#include "Engone/PlatformLayer.h"

struct TrackerType {
    std::string name{};
    u32 size = 0;
    u32 count = 0;
    DynamicArray<TrackLocation> locations{}; // add tracking
    DynamicArray<TrackLocation> freed_locations{}; // del tracking (not sure how reallocations are dealt with)
};

struct Tracker_impl {
    void setTracking(bool enabled);
    void addTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    void delTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    void printTrackedTypes();
    // how much memory the tracker uses
    u32 getMemoryUsage();

    std::unordered_map<size_t, TrackerType> m_trackedTypes{};
    engone::Mutex m_trackLock{};
    bool m_enableTracking = true;
    // bool m_enableTracking = false;
    DynamicArray<TrackLocation> m_uniqueTrackLocations{};

    volatile i32 allocation_id = 1;

    static Tracker_impl* Create();
    static void Destroy(Tracker_impl* tracker);
};


static Tracker_impl* global_tracker = nullptr;

#define ENSURE_GLOBAL_TRACKER if(!global_tracker) { global_tracker = Tracker_impl::Create(); Assert(global_tracker); }

Tracker_impl* Tracker_impl::Create() {
    Tracker_impl* ptr = (Tracker_impl*)engone::Allocate(sizeof(Tracker_impl));
    if(!ptr) return nullptr;
    new(ptr)Tracker_impl();
    return ptr;
}
void Tracker_impl::Destroy(Tracker_impl* tracker) {
    tracker->~Tracker_impl();
    engone::Free(tracker, sizeof(Tracker_impl));
}
void Tracker::DestroyGlobal() {
    if(global_tracker) {
        Tracker_impl::Destroy(global_tracker);
        global_tracker = nullptr;
    }
}
void Tracker::SetTracking(bool enabled) {
    ENSURE_GLOBAL_TRACKER
    global_tracker->setTracking(enabled);
}
void Tracker::AddTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    ENSURE_GLOBAL_TRACKER
    global_tracker->addTracking(typeInfo, size,loc,count);
}
void Tracker::DelTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    ENSURE_GLOBAL_TRACKER
    global_tracker->delTracking(typeInfo, size,loc,count);
}
void Tracker::PrintTrackedTypes(){
    ENSURE_GLOBAL_TRACKER
    global_tracker->printTrackedTypes();
}
u32 Tracker::GetMemoryUsage(){
    // ENSURE_GLOBAL_TRACKER <- don't create memory when we ask for memory usage. We may have queried overall memory usage which would miss this new memory
    if(!global_tracker)
        return 0;
    return global_tracker->getMemoryUsage() + sizeof(Tracker_impl); // the size of the tracker itself must be included here
}
void Tracker_impl::setTracking(bool enabled) {
    m_trackLock.lock();
    m_enableTracking = enabled;
    m_trackLock.unlock();
}
void Tracker_impl::addTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    if(!m_enableTracking)
        return;

    auto id = typeInfo.hash_code();

    int alloc_id = engone::atomic_add(&allocation_id, 1) - 1;

    // @DEBUG
    int alloc_ids[]{
        0,
        // 1, 103, 104, 106, 107
    };
    for(int i=0;i<sizeof(alloc_ids)/sizeof(*alloc_ids);i++) {
        BREAK(alloc_id == alloc_ids[i]);
    }

    m_trackLock.lock();

    bool prev = m_enableTracking;
    m_enableTracking = false;
    
    auto pair = m_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == m_trackedTypes.end()) {
        ptr = &(m_trackedTypes[id] = {});
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
    if(loc.fname){
        ptr->locations.add(loc);
        ptr->locations.last().count = count;
        ptr->locations.last().alloc_id = alloc_id;
    }
    m_enableTracking = prev;
    m_trackLock.unlock();
}
void Tracker_impl::delTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    if(!m_enableTracking)
        return;

    auto id = typeInfo.hash_code();

    m_trackLock.lock();

    int alloc_id = engone::atomic_add(&allocation_id, 1) - 1;

    bool prev = m_enableTracking;
    m_enableTracking = false;

    auto pair = m_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == m_trackedTypes.end()) {
        ptr = &(m_trackedTypes[id] = {});
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
    if(loc.fname) {
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
    m_enableTracking = prev;
    m_trackLock.unlock();
}

void Tracker_impl::printTrackedTypes(){
    using namespace engone;

    m_trackLock.lock();
    bool prev = m_enableTracking;
    m_enableTracking = false;

    u32 totalMemory = 0;
    log::out << log::BLUE<<"Tracked types:\n";
    for(auto& pair : m_trackedTypes) {
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
                if(loc.fname == loc2.fname && loc.line == loc2.line && loc.alloc_id == loc2.alloc_id) {
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
            int len = strlen(loc2.fname);
            int cutoff = len-1;
            int slashCount = 2;
            std::string path = loc2.fname;
            
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
    
    m_enableTracking = prev;
    m_trackLock.unlock();
}
u32 Tracker_impl::getMemoryUsage(){
    u32 memory = 0;
    m_trackLock.lock();
    for(auto& pair : m_trackedTypes) {
        TrackerType* trackType = &pair.second;
        memory += trackType->locations.max * sizeof(TrackLocation);
    }
    m_trackLock.unlock();
    return memory;
}