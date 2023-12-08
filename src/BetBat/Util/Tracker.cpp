#include "BetBat/Util/Tracker.h"

#include "Engone/Util/Array.h"

struct TrackerType {
    std::string name{};
    u32 size = 0;
    u32 count = 0;
    DynamicArray<TrackLocation> locations{};
};
static std::unordered_map<size_t, TrackerType> s_trackedTypes{};
static engone::Mutex s_trackLock{};
static bool s_enabledtracking = true;

void Tracker::SetTracking(bool enabled) {
    s_trackLock.lock();
    s_enabledtracking = enabled;
    s_trackLock.unlock();
}
void Tracker::AddTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    auto id = typeInfo.hash_code();
    
    if(!s_enabledtracking)
        return;

    s_trackLock.lock();

    bool prev = s_enabledtracking;
    s_enabledtracking = false;
    
    auto pair = s_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == s_trackedTypes.end()) {
        ptr = &(s_trackedTypes[id] = {});
        auto name = typeInfo.name();
        ptr->name = name;
        ptr->size = size;
    } else {
        ptr = &pair->second;
    }
    ptr->count+=count;
    #ifdef LOG_TRACKER
    engone::log::out << "Track "<<count<<": "<<typeInfo.name() <<" (left: "<<ptr->count<<")\n";
    #endif
    if(loc.fname){
        ptr->locations.add(loc);
        ptr->locations.last().count = count;
    }
    s_enabledtracking = prev;
    s_trackLock.unlock();
}
void Tracker::DelTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc, u32 count) {
    auto id = typeInfo.hash_code();

    if(!s_enabledtracking)
        return;
    

    s_trackLock.lock();

    bool prev = s_enabledtracking;
    s_enabledtracking = false;

    auto pair = s_trackedTypes.find(id);
    TrackerType* ptr = nullptr;
    if(pair == s_trackedTypes.end()) {
        ptr = &(s_trackedTypes[id] = {});
        auto name = typeInfo.name();
        ptr->name = name;
        ptr->size = size;
    } else {
        ptr = &pair->second;
    }
    ptr->count-=count;
    #ifdef LOG_TRACKER
    engone::log::out << "Unrack "<<count<<": "<<typeInfo.name() <<" (left: "<<ptr->count<<")\n";
    #endif
    if(loc.fname) {
        bool removed = false;
        for(int i=0;i<ptr->locations.size();i++){
            TrackLocation& it = ptr->locations[i];
            if(it.count == count) {
                if(i<ptr->locations.size()-1) {
                    ptr->locations[i] = ptr->locations.last();
                }
                ptr->locations.pop();
                removed = true;
                break;
            }
        }
        // Assert(removed);
    }
    s_enabledtracking = prev;
    s_trackLock.unlock();
}

DynamicArray<TrackLocation> s_uniqueTrackLocations{};
void Tracker::PrintTrackedTypes(){
    using namespace engone;
    s_trackLock.lock();
    bool prev = s_enabledtracking;
    s_enabledtracking = false;

    u32 totalMemory = 0;
    log::out << log::BLUE<<"Tracked types:\n";
    for(auto& pair : s_trackedTypes) {
        TrackerType* trackType = &pair.second;
        if(trackType->count == 0)
            continue;
        if(strcmp(trackType->name.c_str(),"struct "))
            log::out <<" "<<log::LIME<< (trackType->name.c_str()+strlen("struct "))<<log::SILVER <<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
        else
            log::out <<" "<<log::LIME<< trackType->name <<log::SILVER<<"["<<trackType->size<<"]: "<<trackType->count<<"\n";
        s_uniqueTrackLocations.resize(0);
        for(int j=0;j<trackType->locations.size();j++){
            auto& loc = trackType->locations[j];
            bool found = false;
            for(int k=0;k<s_uniqueTrackLocations.size();k++){
                auto& loc2 = trackType->locations[k];
                if(loc.fname == loc2.fname && loc.line == loc2.line) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                s_uniqueTrackLocations.add(loc);
            }
        }
        for(int k=0;k<s_uniqueTrackLocations.size();k++){
            auto& loc2 = trackType->locations[k];
            int len = strlen(loc2.fname);
            int cutoff = len-1;
            int slashCount = 2;
            while(cutoff>0){
                if(loc2.fname[cutoff] == '/' || loc2.fname[cutoff] == '\\'){
                    slashCount--;
                    if(!slashCount) {
                        cutoff++;
                        break;
                    }
                }
                cutoff--;
            }
            // if(len > 25)
            log::out << "  "<<(loc2.fname+(cutoff)) << ":"<<loc2.line<<"\n";
            // else
            //     log::out << "  "<<loc2.fname << ":"<<loc2.line<<"\n";
        }
        totalMemory += trackType->count * trackType->size;
    }
    if(totalMemory != 0)
        log::out << log::RED;
    log::out << "Tracked memory: "<<totalMemory<<"\n";
    
    s_enabledtracking = prev;
    s_trackLock.unlock();
}
u32 Tracker::GetMemoryUsage(){
    u32 memory = 0;
    s_trackLock.lock();
    for(auto& pair : s_trackedTypes) {
        TrackerType* trackType = &pair.second;
        memory += trackType->locations.max * sizeof(TrackLocation);
    }
    s_trackLock.unlock();
    return memory;
}