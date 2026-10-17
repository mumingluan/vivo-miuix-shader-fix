#include "../src/jni_entry_probe.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
static void expect(bool value) { if (!value) throw std::runtime_error("JNI predicate contract assertion failed"); }
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    std::ifstream stream(argv[1],std::ios::binary);
    std::vector<unsigned char> code{std::istreambuf_iterator<char>(stream),{}};
    auto pc=std::stoull(argv[2],nullptr,0), expected=std::stoull(argv[3],nullptr,0);
    using shader_art_probe::generic_jni;
    expect(generic_jni(code.data(),code.size(),pc)==expected);
    expect(generic_jni(code.data(),code.size(),pc+0x10000000)==expected+0x10000000);
    expect(!generic_jni(nullptr,code.size(),pc));
    expect(!generic_jni(code.data(),code.size()-1,pc));
    expect(!generic_jni(code.data(),68,pc));
    size_t rejected=0;
    for(size_t offset=0;offset<code.size();offset+=4) {
        auto changed=code;
        uint32_t invalid=0xd4200000; // BRK: any reached unknown instruction must refuse
        std::memcpy(changed.data()+offset,&invalid,4);
        expect(!generic_jni(changed.data(),changed.size(),pc));++rejected;
    }
    // Both observed forms contain CSET. Inverting its condition must refuse an
    // AND/inequality predicate, even when the referenced code address is valid.
    for(size_t offset=0;offset<code.size();offset+=4) {
        uint32_t word;std::memcpy(&word,code.data()+offset,4);
        if((word&0xffff0fe0)==0x1a9f07e0) {
            auto changed=code;word^=1u<<12;std::memcpy(changed.data()+offset,&word,4);
            expect(!generic_jni(changed.data(),changed.size(),pc));++rejected;
        }
    }
    std::cout<<"PASS generic-jni="<<std::hex<<expected<<std::dec<<" relocation, malformed sizes, "<<rejected<<" rejected instruction/polarity mutations\n";
}
