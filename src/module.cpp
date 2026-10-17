#include <jni.h>
#include <android/log.h>
#include <elf.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include "jni_entry_probe.hpp"
#include "zygisk.hpp"
#include "lsplant.hpp"
#include "dobby.h"
#include "hook_dex.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "VivoMiuixShaderFix", __VA_ARGS__)

namespace {
// Resolve both local and exported ART symbols from its on-disk ELF. No hooks in zygote.
class ArtSymbols {
    void *mapping_ = MAP_FAILED;
    size_t size_ = 0;
    uintptr_t bias_ = 0;
public:
    ArtSymbols() {
        struct Image { uintptr_t bias = 0; std::string path; } image;
        dl_iterate_phdr([](dl_phdr_info *info, size_t, void *opaque) {
            auto &image = *static_cast<Image *>(opaque);
            if (info->dlpi_name && std::string_view(info->dlpi_name).ends_with("/libart.so")) {
                image.bias = info->dlpi_addr; image.path = info->dlpi_name; return 1;
            }
            return 0;
        }, &image);
        if (image.path.empty()) return;
        bias_ = image.bias;
        LOG("ART image=%s bias=%p", image.path.c_str(), reinterpret_cast<void *>(bias_));
        int fd = open(image.path.c_str(), O_RDONLY | O_CLOEXEC);
        struct stat st{};
        if (fd >= 0 && fstat(fd, &st) == 0) {
            size_ = st.st_size;
            mapping_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
        }
        if (fd >= 0) close(fd);
    }
    ~ArtSymbols() { if (mapping_ != MAP_FAILED) munmap(mapping_, size_); }
    void *find(std::string_view name, bool prefix = false,
               const unsigned char **code = nullptr, size_t *code_size = nullptr) const {
        if (mapping_ == MAP_FAILED || size_ < sizeof(Elf64_Ehdr)) return nullptr;
        auto *data = static_cast<const char *>(mapping_);
        auto *eh = reinterpret_cast<const Elf64_Ehdr *>(data);
        if (memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
            eh->e_shentsize != sizeof(Elf64_Shdr) ||
            eh->e_shoff > size_ || size_t(eh->e_shnum) > (size_ - eh->e_shoff) / sizeof(Elf64_Shdr)) return nullptr;
        auto *sections = reinterpret_cast<const Elf64_Shdr *>(data + eh->e_shoff);
        for (size_t i = 0; i < eh->e_shnum; ++i) {
            auto &s = sections[i];
            if ((s.sh_type != SHT_SYMTAB && s.sh_type != SHT_DYNSYM) ||
                s.sh_link >= eh->e_shnum || s.sh_entsize != sizeof(Elf64_Sym) ||
                s.sh_offset > size_ || s.sh_size > size_ - s.sh_offset) continue;
            auto &str = sections[s.sh_link];
            if (str.sh_offset > size_ || str.sh_size > size_ - str.sh_offset) continue;
            auto *symbols = reinterpret_cast<const Elf64_Sym *>(data + s.sh_offset);
            for (size_t j = 0; j < s.sh_size / sizeof(Elf64_Sym); ++j) {
                auto &sym = symbols[j];
                if (!sym.st_value || sym.st_shndx == SHN_UNDEF || sym.st_name >= str.sh_size) continue;
                const char *symbol = data + str.sh_offset + sym.st_name;
                size_t len = strnlen(symbol, str.sh_size - sym.st_name);
                std::string_view value(symbol, len);
                if ((!prefix && value == name) || (prefix && value.starts_with(name))) {
                    if (code || code_size) {
                        if (sym.st_shndx >= eh->e_shnum || !sym.st_size) return nullptr;
                        auto &section = sections[sym.st_shndx];
                        if (!(section.sh_flags & SHF_EXECINSTR) || sym.st_value < section.sh_addr) return nullptr;
                        size_t delta = sym.st_value - section.sh_addr;
                        if (delta > section.sh_size || sym.st_size > section.sh_size - delta ||
                            section.sh_offset > size_ || delta > size_ - section.sh_offset ||
                            sym.st_size > size_ - section.sh_offset - delta) return nullptr;
                        if (code) *code = reinterpret_cast<const unsigned char *>(data + section.sh_offset + delta);
                        if (code_size) *code_size = sym.st_size;
                    }
                    return reinterpret_cast<void *>(bias_ + sym.st_value);
                }
            }
        }
        return nullptr;
    }
    bool executable(uintptr_t address) const {
        struct Query { uintptr_t bias, address; bool found = false; } query{bias_, address};
        dl_iterate_phdr([](dl_phdr_info *info, size_t, void *opaque) {
            auto &query = *static_cast<Query *>(opaque);
            if (info->dlpi_addr != query.bias) return 0;
            for (size_t i = 0; i < info->dlpi_phnum; ++i) {
                const auto &p = info->dlpi_phdr[i];
                if (p.p_type == PT_LOAD && (p.p_flags & PF_X) &&
                    query.address >= info->dlpi_addr + p.p_vaddr &&
                    query.address - info->dlpi_addr - p.p_vaddr < p.p_memsz) query.found = true;
            }
            return 1;
        }, &query);
        return query.found;
    }
};

__attribute__((noinline)) jint probeFirst(JNIEnv *, jclass, jint value) { return value ^ 0x13579; }
__attribute__((noinline)) jint probeSecond(JNIEnv *, jclass, jint value) { return value + 0x2468; }

bool installed = false;
bool check(JNIEnv *env, const char *step) {
    if (!env->ExceptionCheck()) return true;
    LOG("JNI error at %s", step);
    env->ExceptionDescribe(); env->ExceptionClear();
    return false;
}

bool install(JNIEnv *env) {
    if (installed) return true;
    LOG("Initializing in pid=%d", getpid());
    ArtSymbols art;
    auto cl = env->FindClass("java/lang/ClassLoader");
    auto getSystem = env->GetStaticMethodID(cl, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    auto parent = env->CallStaticObjectMethod(cl, getSystem);
    auto dexClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    auto ctor = env->GetMethodID(dexClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto buffer = env->NewDirectByteBuffer(const_cast<unsigned char *>(hook_dex), sizeof(hook_dex));
    auto loader = env->NewObject(dexClass, ctor, buffer, parent);
    auto loadClass = env->GetMethodID(cl, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    auto name = env->NewStringUTF("com.axuan.vivomiuix.ShaderHook");
    auto helper = static_cast<jclass>(env->CallObjectMethod(loader, loadClass, name));
    if (!check(env, "helper load") || !helper) return false;
    auto contractName = env->NewStringUTF("com.axuan.vivomiuix.ShaderContracts");
    auto contracts = static_cast<jclass>(env->CallObjectMethod(loader, loadClass, contractName));
    if (!check(env, "contract helper load") || !contracts) return false;
    auto discover = env->GetStaticMethodID(contracts, "discover", "()[Ljava/lang/Object;");
    auto plan = static_cast<jobjectArray>(env->CallStaticObjectMethod(contracts, discover));
    if (!check(env, "Shader contract discovery") || !plan || env->GetArrayLength(plan) != 2) return false;
    auto target = env->GetObjectArrayElement(plan, 0);
    auto callers = static_cast<jobjectArray>(env->GetObjectArrayElement(plan, 1));
    if (!target || !callers || env->GetArrayLength(callers) == 0) return false;
    const unsigned char *predicateCode = nullptr;
    size_t predicateSize = 0;
    auto predicate = art.find("_ZNK3art11ClassLinker21IsQuickGenericJniStubEPKv", false,
                              &predicateCode, &predicateSize);
    auto quick = predicate ? shader_art_probe::generic_jni(predicateCode, predicateSize,
                          reinterpret_cast<uintptr_t>(predicate)) : 0;
    auto namedQuick = art.find("art_quick_generic_jni_trampoline");
    if (!quick || !art.executable(quick) ||
        (namedQuick && reinterpret_cast<uintptr_t>(namedQuick) != quick)) {
        LOG("Missing/ambiguous generic JNI predicate contract"); return false;
    }
    JNINativeMethod registrations[] = {
        {const_cast<char *>("first"), const_cast<char *>("(I)I"), reinterpret_cast<void *>(probeFirst)},
        {const_cast<char *>("second"), const_cast<char *>("(I)I"), reinterpret_cast<void *>(probeSecond)}
    };
    std::array<lsplant::NativeMethodProbe, 4> probes{};
    const char* probeClasses[]={"com.axuan.vivomiuix.RuntimeLayoutProbe","com.axuan.vivomiuix.RuntimeLayoutPeer"};
    for(size_t group=0;group<2;++group) {
      auto probeName=env->NewStringUTF(probeClasses[group]);
      auto probeClass=static_cast<jclass>(env->CallObjectMethod(loader,loadClass,probeName));
      env->DeleteLocalRef(probeName);
      if(!check(env,"layout probe load")||!probeClass)return false;
      if(env->RegisterNatives(probeClass,registrations,2)!=JNI_OK||!check(env,"register layout probes"))return false;
      for (size_t i = 0; i < 2; ++i) {
        auto id = env->GetStaticMethodID(probeClass, registrations[i].name, registrations[i].signature);
        if (!check(env, "native probe lookup") || !id) return false;
        auto value = env->CallStaticIntMethod(probeClass, id, jint(7));
        if (!check(env, "native probe invocation") || value != (i == 0 ? (7 ^ 0x13579) : (7 + 0x2468))) return false;
        probes[group*2+i] = {env->ToReflectedMethod(probeClass, id, true), registrations[i].fnPtr,
                     reinterpret_cast<void *>(quick),probeClass};
        if (!check(env, "native probe reflection") || !probes[group*2+i].reflected_method) return false;
      }
    }
    lsplant::InitInfo info{
        .inline_hooker = [](void *target, void *replacement) -> void * {
            LOG("Inline hook target=%p", target);
            const size_t page = sysconf(_SC_PAGESIZE);
            uintptr_t start = reinterpret_cast<uintptr_t>(target) & ~(page - 1);
            if (mprotect(reinterpret_cast<void *>(start), page * 2,
                         PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
                LOG("Cannot make hook target writable"); return nullptr;
            }
            void *original = nullptr;
            return DobbyHook(target, replacement, &original) == 0 ? original : nullptr;
        },
        .inline_unhooker = [](void *target) { return DobbyDestroy(target) == 0; },
        .art_symbol_resolver = [&art](std::string_view s) { return art.find(s); },
        .art_symbol_prefix_resolver = [&art](std::string_view s) { return art.find(s, true); },
        .generated_class_name = "VivoMiuixShaderHook_",
        .generated_source_name = "VivoMiuixShaderFix",
        .executable_memory_allocator = {},
        .executable_memory_recycler = {},
        .native_method_probes = probes,
    };
    LOG("Resolving ART hooks from registered JNI witnesses");
    if (!lsplant::Init(env, info)) { check(env, "ART layout discovery"); LOG("LSPlant initialization failed"); return false; }
    auto helperCtor = env->GetMethodID(helper, "<init>", "()V");
    auto hooker = env->NewObject(helper, helperCtor);
    auto callbackId = env->GetMethodID(helper, "callback", "([Ljava/lang/Object;)Ljava/lang/Object;");
    auto callback = env->ToReflectedMethod(helper, callbackId, false);
    auto backup = lsplant::Hook(env, target, hooker, callback);
    if (!check(env, "hook") || !backup) { LOG("Hook failed"); return false; }
    auto backupField = env->GetFieldID(helper, "backup", "Ljava/lang/reflect/Method;");
    env->SetObjectField(hooker, backupField, backup);
    int deoptimized = 0;
    for (int i = 0; i < env->GetArrayLength(callers); ++i) {
        auto member = env->GetObjectArrayElement(callers, i);
        if (lsplant::Deoptimize(env, member)) ++deoptimized;
        env->DeleteLocalRef(member);
    }
    if (!check(env, "caller deoptimization") || deoptimized != env->GetArrayLength(callers)) {
        bool removed = lsplant::UnHook(env, target);
        LOG("Incomplete caller deoptimization; hook removed=%d", removed);
        return false;
    }
    // Keep generated classes and the callback's backup alive throughout this process.
    env->NewGlobalRef(loader); env->NewGlobalRef(hooker); env->NewGlobalRef(backup);
    installed = true;
    LOG("Installed: RuntimeShader copy preserved, other shaders use original method; deoptimized=%d", deoptimized);
    return true;
}

class Module : public zygisk::ModuleBase {
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool target_ = false;
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override { api_ = api; env_ = env; }
    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        auto name = env_->GetStringUTFChars(args->nice_name, nullptr);
        target_ = args->uid % 100000 == 2000 && !strcmp(name, "com.android.shell");
        env_->ReleaseStringUTFChars(args->nice_name, name);
        if (!target_) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (target_) { LOG("Target Shell process pid=%d", getpid()); install(env_); }
    }
    void preServerSpecialize(zygisk::ServerSpecializeArgs *) override {
        api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
};
} // namespace

// Same initialization path, for a one-process live test without restarting zygote.
extern "C" __attribute__((visibility("default"))) bool vivo_shader_fix_test(JNIEnv *env) {
    if (getuid() % 100000 != 2000) return false;
    return install(env);
}

REGISTER_ZYGISK_MODULE(Module)
