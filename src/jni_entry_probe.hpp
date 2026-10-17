#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>

// Bounded interpretation of ART's exported IsQuickGenericJniStub predicate.
// Only pure comparisons, PC-relative constants and the opaque ClassLinker stub
// load are accepted. No instruction is executed and no ClassLinker is dereferenced.
namespace shader_art_probe {
enum class Kind { invalid, argument, object, address, fallback, scalar };
struct Value { uint64_t bits = 0; Kind kind = Kind::invalid; };
inline int64_t sign(uint64_t value, unsigned bits) {
    return static_cast<int64_t>(value << (64 - bits)) >> (64 - bits);
}
inline bool execute(const unsigned char* code, size_t size, uintptr_t pc,
                    uint64_t argument, uint64_t fallback, std::set<uintptr_t>& constants,
                    bool& result) {
    if (!code || size < 8 || size > 64 || size % 4) return false;
    Value regs[32]{};
    regs[0] = {0, Kind::object}; regs[1] = {argument, Kind::argument};
    bool zero = false, flags = false;
    auto condition = [&](unsigned cond, bool& okay) {
        if (!flags || cond > 1) { okay = false; return false; }
        return cond == 0 ? zero : !zero;
    };
    auto compare = [&](Value left, Value right) {
        if (left.kind == Kind::invalid || right.kind == Kind::invalid ||
            left.kind == Kind::object || right.kind == Kind::object) return false;
        if (left.kind == Kind::argument && right.kind == Kind::address) constants.insert(right.bits);
        if (right.kind == Kind::argument && left.kind == Kind::address) constants.insert(left.bits);
        zero = left.bits == right.bits; flags = true; return true;
    };
    for (size_t at = 0, steps = 0; at < size && steps++ < 32; at += 4) {
        uint32_t op; std::memcpy(&op, code + at, 4);
        unsigned rd = op & 31, rn = (op >> 5) & 31, rm = (op >> 16) & 31;
        if (op == 0xd503245f || op == 0xd503201f) continue; // BTI c / NOP
        if ((op & 0x9f000000) == 0x90000000) { // ADRP
            uint64_t imm = ((uint64_t(op >> 5) & 0x7ffff) << 2) | ((op >> 29) & 3);
            regs[rd] = {uint64_t((pc + at) & ~uintptr_t(4095)) + uint64_t(sign(imm, 21) * 4096), Kind::address};
        } else if ((op & 0xff800000) == 0x91000000) { // ADD Xd, Xn, #imm
            if (rd == 31 || rn == 31 || regs[rn].kind != Kind::address) return false;
            regs[rd] = {regs[rn].bits + (uint64_t((op >> 10) & 4095) << ((op & (1u << 22)) ? 12 : 0)), Kind::address};
        } else if ((op & 0xffc00000) == 0xf9400000) { // opaque LDR Xt, [ClassLinker, #imm]
            if (rd == 31 || regs[rn].kind != Kind::object) return false;
            regs[rd] = {fallback, Kind::fallback};
        } else if ((op & 0xffe0fc1f) == 0xeb00001f) { // CMP Xn, Xm, no shift
            if (!compare(regs[rn], regs[rm])) return false;
        } else if ((op & 0xffe00c10) == 0xfa400000) { // CCMP Xn, Xm, #nzcv, cond
            bool okay = true, take = condition((op >> 12) & 15, okay);
            if (!okay) return false;
            if (take) { if (!compare(regs[rn], regs[rm])) return false; }
            else { zero = (op & 4) != 0; flags = true; }
        } else if ((op & 0xffff0fe0) == 0x1a9f07e0) { // CSET Wd, cond (inverted CSINC condition)
            bool okay = true, value = !condition((op >> 12) & 15, okay);
            if (!okay || rd == 31) return false;
            regs[rd] = {uint64_t(value), Kind::scalar};
        } else if ((op & 0xff800000) == 0x52800000) { // MOVZ Wd, #imm
            if (rd == 31 || (op & (3u << 21))) return false;
            regs[rd] = {uint64_t((op >> 5) & 65535), Kind::scalar};
        } else if ((op & 0xff000010) == 0x54000000) { // B.eq / B.ne, bounded forward only
            bool okay = true, take = condition(op & 15, okay);
            int64_t delta = sign((op >> 5) & 0x7ffff, 19) * 4;
            if (!okay || delta <= 0 || uint64_t(delta) >= size - at) return false;
            if (take) at += size_t(delta) - 4;
        } else if (op == 0xd65f03c0) { // RET
            if (regs[0].kind != Kind::scalar || regs[0].bits > 1) return false;
            result = regs[0].bits != 0; return true;
        } else return false;
    }
    return false;
}
inline uintptr_t generic_jni(const unsigned char* code, size_t size, uintptr_t pc) {
    std::set<uintptr_t> candidates;
    bool result;
    if (!execute(code, size, pc, 0, 1, candidates, result) || candidates.size() != 1) return 0;
    uintptr_t candidate = *candidates.begin();
    if (!candidate || candidate % 4) return 0;
    // Require the entire predicate to mean argument == direct stub OR opaque
    // ClassLinker stub, across both branches and equal/unequal operands.
    for (uint64_t other : {uint64_t(candidate), uint64_t(candidate + 0x10000)})
        for (uint64_t input : {uint64_t(candidate), uint64_t(candidate - 4),
                               uint64_t(candidate + 4), other, uint64_t(0), uint64_t(1)}) {
            std::set<uintptr_t> observed;
            if (!execute(code, size, pc, input, other, observed, result) ||
                observed.size() != 1 || *observed.begin() != candidate ||
                result != (input == candidate || input == other)) return 0;
        }
    return candidate;
}
}
