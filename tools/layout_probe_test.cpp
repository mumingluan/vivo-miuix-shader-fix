#include "../vendor/LSPlant/lsplant/src/main/jni/art/runtime/layout_probe.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace lsplant_layout_probe;
void expect(bool value) { if (!value) throw std::runtime_error("layout inference assertion failed"); }
int main() {
    std::array<std::array<uint32_t,8>,4> rows{};
    std::vector<uintptr_t> addresses;
    std::vector<std::pair<uintptr_t,uint32_t>> samples;
    for (size_t i=0;i<rows.size();++i) {
        rows[i][0]=0x12340000;rows[i][3]=0x10000+static_cast<uint32_t>(i)+1;
        auto address=reinterpret_cast<uintptr_t>(rows[i].data());addresses.push_back(address);samples.emplace_back(address,rows[i][3]);
    }
    std::reverse(addresses.begin(),addresses.end());
    expect(stride(addresses,sizeof(uintptr_t))==sizeof(rows[0]));
    expect(stride({1},sizeof(uintptr_t))==0&&stride({1,1},sizeof(uintptr_t))==0);
    expect(stride({0,4096},sizeof(uintptr_t))==0);
    size_t result=0;expect(flags_offset(samples,sizeof(rows[0]),sizeof(uintptr_t),result)&&result==12);
    for (auto& row:rows)row[1]=row[3];
    expect(!flags_offset(samples,sizeof(rows[0]),sizeof(uintptr_t),result));
    for (auto& row:rows)row[1]=0;
    rows[2][3]^=1;expect(!flags_offset(samples,sizeof(rows[0]),sizeof(uintptr_t),result));
    expect(!flags_offset({samples[0]},sizeof(rows[0]),sizeof(uintptr_t),result));
    // Fields may move to either end, and pointer slots need not be the last two.
    for (auto layout : {std::array<size_t,3>{8,24,60}, {32,8,4}, {40,48,20}}) {
        alignas(uintptr_t) std::array<std::array<unsigned char,64>,3> methods{};
        std::vector<std::pair<uintptr_t,uintptr_t>> data, entry;
        std::vector<std::pair<uintptr_t,uint32_t>> flags;
        for (size_t i=0;i<methods.size();++i) {
            uintptr_t native=0x12340000+i*16, quick=0x56780000;
            uint32_t raw=0x10009+static_cast<uint32_t>(i)*2;
            auto address=reinterpret_cast<uintptr_t>(methods[i].data());
            std::memcpy(methods[i].data()+layout[0],&native,sizeof(native));
            std::memcpy(methods[i].data()+layout[1],&quick,sizeof(quick));
            std::memcpy(methods[i].data()+layout[2],&raw,sizeof(raw));
            data.emplace_back(address,native); entry.emplace_back(address,quick); flags.emplace_back(address,raw);
        }
        expect(pointer_offset(data,64,result)&&result==layout[0]);
        expect(pointer_offset(entry,64,result)&&result==layout[1]);
        expect(flags_offset(flags,64,sizeof(uintptr_t),result)&&result==layout[2]);
        for (size_t i=0;i<methods.size();++i) std::memcpy(methods[i].data()+56,&data[i].second,sizeof(uintptr_t));
        expect(!pointer_offset(data,64,result)); // ambiguous pointer copies
        expect(!pointer_offset(data,129,result));
        expect(!pointer_offset({data[0]},64,result));
        data[1].second^=4;expect(!pointer_offset(data,64,result));
    }
    for(size_t class_slot:{0u,4u,8u,12u,24u,52u,60u}) {
        alignas(uintptr_t) std::array<std::array<unsigned char,64>,6> methods{};
        std::vector<ClassWitness> classes;
        for(size_t i=0;i<methods.size();++i) {
            uint32_t group=uint32_t(i/2),reference=0x12000000+group*0x10000,flags=0x1000+group*4,index=uint32_t(i+1);
            uintptr_t native=(uintptr_t(0x2000+group*4)<<32)|(0x500000+i*16),quick=(uintptr_t(0x3000+group*4)<<32)|0x800000;
            std::memcpy(methods[i].data()+class_slot,&reference,sizeof(reference));
            std::memcpy(methods[i].data()+16,&flags,sizeof(flags));std::memcpy(methods[i].data()+20,&index,sizeof(index));
            std::memcpy(methods[i].data()+32,&native,sizeof(native));std::memcpy(methods[i].data()+40,&quick,sizeof(quick));
            classes.push_back({reinterpret_cast<uintptr_t>(methods[i].data()),group});
        }
        expect(class_offset(classes,64,sizeof(uintptr_t),16,32,40,result)&&result==class_slot);
        size_t duplicate=class_slot==0?4:0;
        for(auto& method:methods)std::memcpy(method.data()+duplicate,method.data()+class_slot,sizeof(uint32_t));
        result=999;expect(!class_offset(classes,64,sizeof(uintptr_t),16,32,40,result)&&result==999);
        for(auto& method:methods)std::memset(method.data()+duplicate,0,sizeof(uint32_t));
        auto fewer=classes;fewer.pop_back();expect(!class_offset(fewer,64,sizeof(uintptr_t),16,32,40,result));
        fewer.assign(classes.begin(),classes.begin()+4);expect(!class_offset(fewer,64,sizeof(uintptr_t),16,32,40,result));
        auto repeated=classes;repeated.push_back(classes[0]);expect(!class_offset(repeated,64,sizeof(uintptr_t),16,32,40,result));
        auto wrong=classes;wrong[1].identity=1;expect(!class_offset(wrong,64,sizeof(uintptr_t),16,32,40,result));
        uint32_t saved=0;std::memcpy(&saved,methods[0].data()+class_slot,sizeof(saved));
        uint32_t invalid=0;std::memcpy(methods[0].data()+class_slot,&invalid,sizeof(invalid));expect(!class_offset(classes,64,sizeof(uintptr_t),16,32,40,result));
        invalid=saved|1;std::memcpy(methods[0].data()+class_slot,&invalid,sizeof(invalid));expect(!class_offset(classes,64,sizeof(uintptr_t),16,32,40,result));
        std::memcpy(methods[0].data()+class_slot,&saved,sizeof(saved));
        expect(!class_offset(classes,129,sizeof(uintptr_t),16,32,40,result));
        expect(!class_offset(classes,64,sizeof(uintptr_t),63,32,40,result));
        expect(!class_offset(classes,64,sizeof(uintptr_t),16,32,32,result));
        for(size_t i=2;i<4;++i)std::memcpy(methods[i].data()+class_slot,&saved,sizeof(saved));
        expect(!class_offset(classes,64,sizeof(uintptr_t),16,32,40,result));
    }
    alignas(uintptr_t) std::array<unsigned char,272> separate{};
    auto start=reinterpret_cast<uintptr_t>(separate.data());
    expect(stride({start,start+64},sizeof(uintptr_t))==64&&stride({start+136,start+200},sizeof(uintptr_t))==64);
    expect(stride({start,start+64,start+136,start+200},sizeof(uintptr_t))==0); // inter-class allocation gaps are not structure size
    std::cout<<"PASS stride, moved/reordered data/entry/flags and zero/nonzero declaring-class slots, independent class groups, pointer-high-word exclusion, ambiguity, identity/representation mismatch, duplicate/missing witnesses, inter-class gaps and bounds\n";
}
