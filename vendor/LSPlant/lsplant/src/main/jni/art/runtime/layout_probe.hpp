#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

// Local adaptation: infer from several actual ArtMethod observations; never
// substitute an assumed access-flags offset when observations disagree.
namespace lsplant_layout_probe {
struct ClassWitness { uintptr_t method; uint32_t identity; };
inline bool class_offset(const std::vector<ClassWitness>& samples,size_t method_size,size_t pointer_size,
                         size_t flags,size_t data,size_t entry,size_t& output) {
    if(samples.size()<6||samples.size()>256||method_size>128||method_size<2*pointer_size+2*sizeof(uint32_t)
            ||(pointer_size!=4&&pointer_size!=8))return false;
    const std::pair<size_t,size_t> blocked[]={{flags,sizeof(uint32_t)},{data,pointer_size},{entry,pointer_size}};
    for(size_t i=0;i<3;i++) {
        if(blocked[i].first>method_size||blocked[i].second>method_size-blocked[i].first)return false;
        for(size_t j=0;j<i;j++)if(blocked[i].first<blocked[j].first+blocked[j].second&&blocked[j].first<blocked[i].first+blocked[i].second)return false;
    }
    std::map<uint32_t,size_t> groups;std::set<uintptr_t> addresses;
    for(const auto& s:samples) {
        if(!s.method||s.method%alignof(uint32_t)||s.method>std::numeric_limits<uintptr_t>::max()-method_size||!addresses.insert(s.method).second)return false;
        ++groups[s.identity];
    }
    if(groups.size()<3)return false;
    for(const auto& group:groups)if(group.second<2)return false;
    size_t matches=0,selected=0;
    for(size_t offset=0;offset+sizeof(uint32_t)<=method_size;offset+=alignof(uint32_t)) {
        bool excluded=false;for(auto slot:blocked)if(offset<slot.first+slot.second&&slot.first<offset+sizeof(uint32_t))excluded=true;
        if(excluded)continue;
        std::map<uint32_t,uint32_t> references;bool valid=true;
        for(const auto& s:samples) {
            uint32_t word=0;std::memcpy(&word,reinterpret_cast<const void*>(s.method+offset),sizeof(word));
            if(!word||word%alignof(uint32_t)){valid=false;break;}
            auto [it,inserted]=references.emplace(s.identity,word);
            if(!inserted&&it->second!=word){valid=false;break;}
        }
        std::set<uint32_t> values;for(const auto& group:references)values.insert(group.second);
        if(!valid||values.size()!=groups.size())continue;
        // A second read must retain the same equivalence classes. JNI refs
        // retain the classes; a concurrent changing observation is refused.
        for(const auto& s:samples) {
            uint32_t word=0;std::memcpy(&word,reinterpret_cast<const void*>(s.method+offset),sizeof(word));
            if(word!=references.at(s.identity)){valid=false;break;}
        }
        if(valid){selected=offset;++matches;}
    }
    if(matches!=1)return false;
    output=selected;return true;
}
inline size_t stride(std::vector<uintptr_t> addresses, size_t pointer_size) {
    if (addresses.size() < 2) return 0;
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    if (addresses.size() < 2) return 0;
    size_t result = 0;
    for (size_t i = 1; i < addresses.size(); ++i)
        result = std::gcd(result, static_cast<size_t>(addresses[i] - addresses[i - 1]));
    // Bounds protect probe reads; they do not choose a layout or OS version.
    return result >= 2 * pointer_size + sizeof(uint32_t) && result <= 128
            && result % sizeof(uint32_t) == 0 ? result : 0;
}
inline bool flags_offset(const std::vector<std::pair<uintptr_t,uint32_t>>& samples,
                         size_t method_size, size_t pointer_size, size_t& output) {
    if (samples.size() < 2 || method_size < 2 * pointer_size + sizeof(uint32_t) || method_size > 128) return false;
    size_t matches = 0;
    for (size_t offset = 0; offset + sizeof(uint32_t) <= method_size;
            offset += sizeof(uint32_t)) {
        bool match = true;
        for (const auto& sample : samples) {
            uint32_t value = 0;
            if (!sample.first || !sample.second) { match = false; break; }
            std::memcpy(&value, reinterpret_cast<const void*>(sample.first + offset), sizeof(value));
            if (value != sample.second) { match = false; break; }
        }
        if (match) { output = offset; ++matches; }
    }
    return matches == 1;
}
inline bool pointer_offset(const std::vector<std::pair<uintptr_t,uintptr_t>>& samples,
                           size_t method_size, size_t& output) {
    if (samples.size() < 2 || method_size > 128 || method_size < 2 * sizeof(uintptr_t) + sizeof(uint32_t)) return false;
    size_t matches = 0;
    for (size_t offset = 0; offset + sizeof(uintptr_t) <= method_size; offset += alignof(uintptr_t)) {
        bool match = true;
        for (const auto& sample : samples) {
            uintptr_t value = 0;
            if (!sample.first || !sample.second) { match = false; break; }
            std::memcpy(&value, reinterpret_cast<const void*>(sample.first + offset), sizeof(value));
            if (value != sample.second) { match = false; break; }
        }
        if (match) { output = offset; ++matches; }
    }
    return matches == 1;
}
}
