module;

#include <atomic>
#include <memory>
#include <string>

#include "logging.hpp"
#include "layout_probe.hpp"

export module lsplant:art_method;

import :common;
import hook_helper;

export namespace lsplant::art {

struct alignas(4) [[gnu::packed]] QuickMethodFrameInfo {
    [[maybe_unused]] uint32_t frame_size_in_bytes;
    [[maybe_unused]] uint32_t core_spill_mask;
    [[maybe_unused]] uint32_t fp_spill_mask;
};

class ArtMethod {
    inline static ArtMethod *abstract_method_{};

    inline static auto SetNotIntrinsic_ =
        "_ZN3art9ArtMethod15SetNotIntrinsicEv"_sym.as<void (ArtMethod::*)()>;

    inline static auto PrettyMethod_ =
            "_ZN3art9ArtMethod12PrettyMethodEPS0_b"_sym.as<std::string(ArtMethod::*)(bool)>;

    inline static auto PrettyMethodStatic_ =
            "_ZN3art12PrettyMethodEPNS_9ArtMethodEb"_sym.as<std::string(ArtMethod *thiz, bool with_signature)>;

    inline static auto PrettyMethodMirror_ =
            "_ZN3art12PrettyMethodEPNS_6mirror9ArtMethodEb"_sym.as<std::string(ArtMethod *thiz, bool with_signature)>;

    inline static auto GetMethodShortyL_ =
            "_ZN3artL15GetMethodShortyEP7_JNIEnvP10_jmethodID"_sym.as<const char *(JNIEnv *env, jmethodID method)>;

    inline static auto GetMethodShorty_ =
            "_ZN3art15GetMethodShortyEP7_JNIEnvP10_jmethodID"_sym.as<const char *(JNIEnv *env, jmethodID mid)>;

    inline static auto ThrowInvocationTimeError_ =
            "_ZN3art9ArtMethod24ThrowInvocationTimeErrorEv"_sym.as<void(ArtMethod::*)()>;

    inline static auto art_interpreter_to_compiled_code_bridge_ =
            "artInterpreterToCompiledCodeBridge"_sym.as<void()>;

    inline static auto GetQuickFrameInfo_ =
        "_ZN3art9ArtMethod17GetQuickFrameInfoEv"_sym.hook->*
        []<MemBackup auto backup>(ArtMethod *thiz) static -> QuickMethodFrameInfo {
        if (proxied_backup_methods_.contains(thiz)) [[unlikely]] {
            return backup(abstract_method_);
        }
        return backup(thiz);
    };

    inline void ThrowInvocationTimeError() {
        if (ThrowInvocationTimeError_) {
            [[likely]] ThrowInvocationTimeError_(this);
        }
    }

public:
    inline static const char *GetMethodShorty(JNIEnv *env, jobject method) {
        if (GetMethodShortyL_) {
            return GetMethodShortyL_(env, env->FromReflectedMethod(method));
        }
        return GetMethodShorty_(env, env->FromReflectedMethod(method));
    }

    void SetNonCompilable() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccCompileDontBother;
        access_flags &= ~kAccPreCompiled;
        SetAccessFlags(access_flags);
    }

    void ClearFastInterpretFlag() {
        auto access_flags = GetAccessFlags();
        access_flags &= ~kAccFastInterpreterToInterpreterInvoke;
        SetAccessFlags(access_flags);
    }

    void SetPrivate() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccPrivate;
        access_flags &= ~kAccProtected;
        access_flags &= ~kAccPublic;
        SetAccessFlags(access_flags);
    }

    void SetPublic() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccPublic;
        access_flags &= ~kAccProtected;
        access_flags &= ~kAccPrivate;
        SetAccessFlags(access_flags);
    }

    void SetProtected() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccProtected;
        access_flags &= ~kAccPrivate;
        access_flags &= ~kAccPublic;
        SetAccessFlags(access_flags);
    }

    void SetNonFinal() {
        auto access_flags = GetAccessFlags();
        access_flags &= ~kAccFinal;
        SetAccessFlags(access_flags);
    }

    void SetNative() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccNative;
        SetAccessFlags(access_flags);
    }

    void SetNonNative() {
        auto access_flags = GetAccessFlags();
        access_flags &= ~kAccNative;
        SetAccessFlags(access_flags);
    }

    void SetNonIntrinsic() {
        if (SetNotIntrinsic_) [[likely]] {
            SetNotIntrinsic_(this);
        } else if (kAccIntrinsic) [[likely]] {
            auto access_flags = GetAccessFlags();
            access_flags &= ~kAccIntrinsic;
            SetAccessFlags(access_flags);
        }
    }

    bool IsPrivate() { return GetAccessFlags() & kAccPrivate; }
    bool IsProtected() { return GetAccessFlags() & kAccProtected; }
    bool IsPublic() { return GetAccessFlags() & kAccPublic; }
    bool IsFinal() { return GetAccessFlags() & kAccFinal; }
    bool IsStatic() { return GetAccessFlags() & kAccStatic; }
    bool IsNative() { return GetAccessFlags() & kAccNative; }
    bool IsAbstract() { return GetAccessFlags() & kAccAbstract; }
    bool IsConstructor() { return GetAccessFlags() & kAccConstructor; }

    void CopyFrom(const ArtMethod *other) { memcpy(this, other, art_method_size); }

    void SetEntryPoint(void *entry_point) {
        *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + entry_point_offset) =
            entry_point;
        if (interpreter_entry_point_offset) [[unlikely]] {
            *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) +
                                       interpreter_entry_point_offset) =
                reinterpret_cast<void *>(&art_interpreter_to_compiled_code_bridge_);
        }
    }

    void *GetEntryPoint() {
        return *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + entry_point_offset);
    }

    void *GetData() {
        return *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + data_offset);
    }

    void SetData(void *data) {
        *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + data_offset) = data;
    }

    uint32_t GetAccessFlags() {
        return (reinterpret_cast<const std::atomic<uint32_t> *>(reinterpret_cast<uintptr_t>(this) +
                                                                access_flags_offset))
            ->load(std::memory_order_relaxed);
    }

    void SetAccessFlags(uint32_t flags) {
        return (reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<uintptr_t>(this) +
                                                          access_flags_offset))
            ->store(flags, std::memory_order_relaxed);
    }

    std::string PrettyMethod(bool with_signature = true) {
        if (PrettyMethod_) [[likely]]
            return PrettyMethod_(this, with_signature);
        if (PrettyMethodStatic_) return PrettyMethodStatic_(this, with_signature);
        if (PrettyMethodMirror_) return PrettyMethodMirror_(this, with_signature);
        return "null sym";
    }

    mirror::Class *GetDeclaringClass() {
        return reinterpret_cast<mirror::Class *>(*reinterpret_cast<uint32_t *>(
            reinterpret_cast<uintptr_t>(this) + declaring_class_offset));
    }

    std::unique_ptr<ArtMethod> Clone() {
        auto *method = reinterpret_cast<ArtMethod*>(::operator new(art_method_size));
        method->CopyFrom(this);
        return std::unique_ptr<ArtMethod>(method);
    }

    void BackupTo(ArtMethod *backup) {
        SetNonCompilable();

        // copy after setNonCompilable
        backup->CopyFrom(this);

        ClearFastInterpretFlag();

        if (!backup->IsStatic()) backup->SetPrivate();
    }

    static art::ArtMethod *FromReflectedMethod(JNIEnv *env, jobject method) {
        if (art_method_field) [[likely]] {
            return reinterpret_cast<art::ArtMethod *>(
                JNI_GetLongField(env, method, art_method_field));
        } else {
            return reinterpret_cast<art::ArtMethod *>(env->FromReflectedMethod(method));
        }
    }

    static bool Init(JNIEnv *env, const HookHandler handler) {
        auto sdk_int = GetAndroidApiLevel();
        ScopedLocalRef<jclass> executable{env, nullptr};
        if (sdk_int >= kSdkOreo) {
            executable = JNI_FindClass(env, "java/lang/reflect/Executable");
        } else if (sdk_int >= kSdkMarshmallow) {
            executable = JNI_FindClass(env, "java/lang/reflect/AbstractMethod");
        } else {
            executable = JNI_FindClass(env, "java/lang/reflect/ArtMethod");
        }
        if (!executable) {
            LOGE("Failed to find Executable/AbstractMethod/ArtMethod");
            return false;
        }

        std::vector<jfieldID> flags_fields;
        if (sdk_int >= kSdkMarshmallow) [[likely]] {
            // Discover reflected storage by primitive type. Private field names
            // may change independently of the public reflection API.
            auto class_class = env->FindClass("java/lang/Class");
            auto field_class = env->FindClass("java/lang/reflect/Field");
            auto long_class = env->FindClass("java/lang/Long");
            auto int_class = env->FindClass("java/lang/Integer");
            auto get_fields = env->GetMethodID(class_class, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
            auto get_type = env->GetMethodID(field_class, "getType", "()Ljava/lang/Class;");
            auto get_modifiers = env->GetMethodID(field_class, "getModifiers", "()I");
            auto long_type = env->GetStaticObjectField(long_class, env->GetStaticFieldID(long_class, "TYPE", "Ljava/lang/Class;"));
            auto int_type = env->GetStaticObjectField(int_class, env->GetStaticFieldID(int_class, "TYPE", "Ljava/lang/Class;"));
            auto fields = static_cast<jobjectArray>(env->CallObjectMethod(executable.get(), get_fields));
            if (env->ExceptionCheck() || !fields) return false;
            size_t pointer_fields = 0;
            for (jsize i = 0; i < env->GetArrayLength(fields); ++i) {
                auto field = env->GetObjectArrayElement(fields, i);
                auto type = env->CallObjectMethod(field, get_type);
                if ((env->CallIntMethod(field, get_modifiers) & 8) == 0) {
                    if (env->IsSameObject(type, long_type)) { art_method_field = env->FromReflectedField(field); ++pointer_fields; }
                    if (env->IsSameObject(type, int_type)) flags_fields.push_back(env->FromReflectedField(field));
                }
                env->DeleteLocalRef(type); env->DeleteLocalRef(field);
            }
            env->DeleteLocalRef(fields); env->DeleteLocalRef(long_type); env->DeleteLocalRef(int_type);
            env->DeleteLocalRef(long_class); env->DeleteLocalRef(int_class);
            env->DeleteLocalRef(field_class); env->DeleteLocalRef(class_class);
            if (env->ExceptionCheck() || pointer_fields != 1 || !art_method_field || flags_fields.empty()) {
                LOGE("Missing/ambiguous reflected primitive storage"); return false;
            }
        }

        auto throwable = JNI_FindClass(env, "java/lang/Throwable");
        if (!throwable) {
            LOGE("Failed to find Throwable");
            return false;
        }
        auto clazz = JNI_FindClass(env, "java/lang/Class");
        static_assert(std::is_same_v<decltype(clazz)::BaseType, jclass>);
        jmethodID get_declared_constructors = JNI_GetMethodID(env, clazz, "getDeclaredConstructors",
                                                              "()[Ljava/lang/reflect/Constructor;");
        const auto constructors =
            JNI_Cast<jobjectArray>(JNI_CallObjectMethod(env, throwable, get_declared_constructors));
        if (constructors.size() < 2) {
            LOGE("Throwable has less than 2 constructors");
            return false;
        }
        auto first_ctor = constructors[0];
        auto *first = FromReflectedMethod(env, first_ctor.get());
        std::vector<uintptr_t> method_addresses;
        for (size_t i = 0; i < constructors.size(); ++i) {
            auto address = reinterpret_cast<uintptr_t>(FromReflectedMethod(env, constructors[i].get()));
            if (!address || address % sizeof(uint32_t) || env->ExceptionCheck()) return false;
            method_addresses.push_back(address);
        }
        art_method_size = lsplant_layout_probe::stride(method_addresses, kPointerSize);
        if (!art_method_size) { LOGE("Unable to infer a bounded ArtMethod stride"); return false; }
        LOGD("ArtMethod size: %zu", art_method_size);

        // Registered JNI functions and the actual ART generic-JNI predicate
        // provide independent data/quick-entry witnesses. Never guess the last
        // slots in the structure when a layout cannot be established.
        auto probes = handler.native_method_probes();
        if (probes.size() < 4 || probes.size() > 32) { LOGE("Missing bounded independent JNI class witnesses"); return false; }
        auto get_declaring_class = JNI_GetMethodID(env, executable, "getDeclaringClass", "()Ljava/lang/Class;");
        if (!get_declaring_class || env->ExceptionCheck()) return false;
        std::vector<uintptr_t> native_addresses;
        std::vector<jclass> native_classes;
        std::vector<size_t> native_groups;
        std::vector<std::vector<uintptr_t>> native_class_addresses;
        std::vector<std::pair<uintptr_t,uintptr_t>> data_samples, entry_samples;
        for (const auto& probe : probes) {
            if (!probe.reflected_method || !probe.native_function || !probe.quick_entry || !probe.declaring_class) return false;
            auto actual_class = JNI_CallObjectMethod(env, probe.reflected_method, get_declaring_class);
            if (env->ExceptionCheck() || !actual_class || !JNI_IsSameObject(env,actual_class,probe.declaring_class)) return false;
            size_t group=0;
            while(group<native_classes.size()&&!env->IsSameObject(native_classes[group],probe.declaring_class))++group;
            if(group==native_classes.size()){native_classes.push_back(probe.declaring_class);native_class_addresses.emplace_back();}
            auto address = reinterpret_cast<uintptr_t>(FromReflectedMethod(env, probe.reflected_method));
            if (!address || address % sizeof(uint32_t) || env->ExceptionCheck()) return false;
            native_addresses.push_back(address);
            native_groups.push_back(group);native_class_addresses[group].push_back(address);
            data_samples.emplace_back(address, reinterpret_cast<uintptr_t>(probe.native_function));
            entry_samples.emplace_back(address, reinterpret_cast<uintptr_t>(probe.quick_entry));
        }
        if (native_classes.size()<2 ||
            !std::all_of(native_class_addresses.begin(),native_class_addresses.end(),[](const auto& group){return lsplant_layout_probe::stride(group,kPointerSize)==art_method_size;}) ||
            data_samples[0].second == data_samples[1].second ||
            !lsplant_layout_probe::pointer_offset(data_samples, art_method_size, data_offset) ||
            !lsplant_layout_probe::pointer_offset(entry_samples, art_method_size, entry_point_offset) ||
            data_offset == entry_point_offset) {
            LOGE("Missing/ambiguous JNI data/quick-entry layout"); return false;
        }

        if (sdk_int >= kSdkMarshmallow) [[likely]] {
            auto modifiers_class = env->FindClass("java/lang/reflect/Modifier");
            auto mask_method = env->GetStaticMethodID(modifiers_class, "methodModifiers", "()I");
            auto public_mask = env->CallStaticIntMethod(modifiers_class, mask_method);
            auto get_modifiers = env->GetMethodID(executable.get(), "getModifiers", "()I");
            if (env->ExceptionCheck() || !public_mask || !get_modifiers) return false;
            size_t matches = 0;
            for (auto field : flags_fields) {
                bool metadata_match = true;
                std::vector<std::pair<uintptr_t,uint32_t>> samples;
                auto observe = [&](jobject method, uintptr_t address) {
                    uint32_t raw = env->GetIntField(method, field);
                    uint32_t published = env->CallIntMethod(method, get_modifiers);
                    if (!raw || (raw & public_mask) != (published & public_mask)) metadata_match = false;
                    samples.emplace_back(address, raw);
                };
                for (size_t i = 0; i < constructors.size(); ++i) observe(constructors[i].get(), method_addresses[i]);
                for (size_t i = 0; i < probes.size(); ++i) observe(probes[i].reflected_method, native_addresses[i]);
                size_t offset = 0;
                if (metadata_match && lsplant_layout_probe::flags_offset(samples, art_method_size, kPointerSize, offset) &&
                    (offset + sizeof(uint32_t) <= data_offset || offset >= data_offset + kPointerSize) &&
                    (offset + sizeof(uint32_t) <= entry_point_offset || offset >= entry_point_offset + kPointerSize)) {
                    access_flags_offset = offset; ++matches;
                }
            }
            env->DeleteLocalRef(modifiers_class);
            if (env->ExceptionCheck() || matches != 1) {
                LOGE("Missing/ambiguous reflected flags metadata and ArtMethod offset"); return false;
            }
            // Public declaring-class identity ties together unrelated methods
            // from two helper classes and two independent boot classes. Known
            // flags/data/entry slots are excluded, including pointer high words.
            auto object_class=JNI_FindClass(env,"java/lang/Object");
            auto get_methods=JNI_GetMethodID(env,clazz,"getDeclaredMethods","()[Ljava/lang/reflect/Method;");
            if(!object_class||!get_methods||env->ExceptionCheck())return false;
            auto object_methods=JNI_Cast<jobjectArray>(JNI_CallObjectMethod(env,object_class,get_methods));
            if(env->ExceptionCheck()||!object_methods||object_methods.size()<2||object_methods.size()>128)return false;
            for(auto native_class:native_classes)if(env->IsSameObject(native_class,throwable.get())||env->IsSameObject(native_class,object_class.get()))return false;
            std::vector<lsplant_layout_probe::ClassWitness> class_samples;
            auto observe_class=[&](jobject method,uintptr_t address,jclass expected,uint32_t group) {
                auto declared=JNI_CallObjectMethod(env,method,get_declaring_class);
                if(env->ExceptionCheck()||!declared||!JNI_IsSameObject(env,declared,expected)||!address||address%sizeof(uint32_t))return false;
                uint32_t flags=0;std::memcpy(&flags,reinterpret_cast<const void*>(address+access_flags_offset),sizeof(flags));
                uint32_t published=env->CallIntMethod(method,get_modifiers);
                if(env->ExceptionCheck()||(flags&public_mask)!=(published&public_mask))return false;
                class_samples.push_back({address,group});return true;
            };
            for(size_t i=0;i<constructors.size();++i)if(!observe_class(constructors[i].get(),method_addresses[i],throwable.get(),0))return false;
            for(size_t i=0;i<probes.size();++i)if(!observe_class(probes[i].reflected_method,native_addresses[i],probes[i].declaring_class,uint32_t(native_groups[i]+2)))return false;
            for(size_t i=0;i<object_methods.size();++i) {
                auto method=object_methods[i];auto address=reinterpret_cast<uintptr_t>(FromReflectedMethod(env,method.get()));
                if(!observe_class(method.get(),address,object_class.get(),1))return false;
            }
            if(!lsplant_layout_probe::class_offset(class_samples,art_method_size,kPointerSize,access_flags_offset,data_offset,entry_point_offset,declaring_class_offset)) {
                LOGE("Missing/ambiguous declaring-class reference identity layout");return false;
            }
        } else {
            auto art_field = JNI_FindClass(env, "java/lang/reflect/ArtField");
            auto field = JNI_FindClass(env, "java/lang/reflect/Field");
            auto art_field_field =
                JNI_GetFieldID(env, field, "artField", "Ljava/lang/reflect/ArtField;");
            auto field_offset = JNI_GetFieldID(env, art_field, "offset", "I");
            auto get_offset_from_art_method = [&](const char *name, const char *sig) {
                return JNI_GetIntField(
                    env,
                    JNI_GetObjectField(
                        env,
                        JNI_ToReflectedField(env, executable,
                                             JNI_GetFieldID(env, executable, name, sig), false),
                        art_field_field),
                    field_offset);
            };
            access_flags_offset = get_offset_from_art_method("accessFlags", "I");
            declaring_class_offset =
                get_offset_from_art_method("declaringClass", "Ljava/lang/Class;");
            if (sdk_int == kSdkLollipop) {
                entry_point_offset =
                    get_offset_from_art_method("entryPointFromQuickCompiledCode", "J");
                interpreter_entry_point_offset =
                    get_offset_from_art_method("entryPointFromInterpreter", "J");
                data_offset = get_offset_from_art_method("entryPointFromJni", "J");
            }
        }
        LOGD("ArtMethod::declaring_class offset: %zu", declaring_class_offset);
        LOGD("ArtMethod::entrypoint offset: %zu", entry_point_offset);
        LOGD("ArtMethod::data offset: %zu", data_offset);
        LOGD("ArtMethod::access_flags offset: %zu", access_flags_offset);

        if (sdk_int < kSdkR) {
            kAccPreCompiled = 0;
        } else if (sdk_int >= kSdkS) {
            kAccPreCompiled = 0x00800000;
        }
        if (sdk_int < kSdkQ) kAccFastInterpreterToInterpreterInvoke = 0;
        if (sdk_int < kSdkOreo) kAccIntrinsic = 0;

        if (sdk_int >= kSdkPie && !handler(SetNotIntrinsic_)) {
            LOGW("Failed to find SetNotIntrinsic, use hard-coded kAccIntrinsic instead");
        }

        if (!handler(GetMethodShortyL_, true, GetMethodShorty_)) {
            LOGE("Failed to find GetMethodShorty");
            return false;
        }

        handler(PrettyMethod_, PrettyMethodStatic_, PrettyMethodMirror_);

        if (sdk_int <= kSdkOreo) [[unlikely]] {
            auto abstract_method_error = JNI_FindClass(env, "java/lang/AbstractMethodError");
            if (!abstract_method_error) {
                LOGE("Failed to find AbstractMethodError");
                return false;
            }
            if (sdk_int == kSdkOreo) [[unlikely]] {
                auto executable_get_name =
                    JNI_GetMethodID(env, executable, "getName", "()Ljava/lang/String;");
                if (!executable_get_name) {
                    LOGE("Failed to find Executable.getName");
                    return false;
                }
                handler(ThrowInvocationTimeError_);
                auto abstract_method = FromReflectedMethod(
                    env, JNI_ToReflectedMethod(env, executable, executable_get_name, false).get());
                uint32_t access_flags = abstract_method->GetAccessFlags();
                abstract_method->SetAccessFlags(access_flags | kAccDefaultConflict);
                abstract_method->ThrowInvocationTimeError();
                abstract_method->SetAccessFlags(access_flags);
            }
            if (auto exception = env->ExceptionOccurred();
                env->ExceptionClear(),
                (!exception || JNI_IsInstanceOf(env, exception, abstract_method_error)))
                [[likely]] {
                kAccCompileDontBother = kAccDefaultConflict;
            }
        }
        if (sdk_int == kSdkMarshmallow) [[unlikely]] {
            auto executable_get_name =
                JNI_GetMethodID(env, executable, "getName", "()Ljava/lang/String;");
            if (!executable_get_name) {
                LOGE("Failed to find Executable.getName");
                return false;
            }
            abstract_method_ = FromReflectedMethod(
                env, JNI_ToReflectedMethod(env, executable, executable_get_name, false).get());
            if (!abstract_method_ || !abstract_method_->IsAbstract()) [[unlikely]] {
                LOGW("Abstract method Executable.getName not found");
            } else if (!handler(GetQuickFrameInfo_)) [[unlikely]] {
                LOGW("Failed to hook GetQuickFrameInfo, hooking proxy method may crash");
            }
        }
        if (sdk_int < kSdkNougat) {
            kAccCompileDontBother = 0;
        }
        if (sdk_int <= kSdkMarshmallow) [[unlikely]] {
            if (!handler(art_interpreter_to_compiled_code_bridge_)) {
                return false;
            }
            if (sdk_int >= kSdkLollipopMr1) {
                interpreter_entry_point_offset = entry_point_offset - 2 * kPointerSize;
            }
        }

        return true;
    }

    static size_t GetEntryPointOffset() { return entry_point_offset; }

    constexpr static uint32_t kAccPublic = 0x0001;           // class, field, method, ic
    constexpr static uint32_t kAccPrivate = 0x0002;          // field, method, ic
    constexpr static uint32_t kAccProtected = 0x0004;        // field, method, ic
    constexpr static uint32_t kAccStatic = 0x0008;           // field, method, ic
    constexpr static uint32_t kAccNative = 0x0100;           // method
    constexpr static uint32_t kAccFinal = 0x0010;            // class, field, method, ic
    constexpr static uint32_t kAccAbstract = 0x0400;         // class, method, ic
    constexpr static uint32_t kAccConstructor = 0x00010000;  // method (dex only) <(cl)init>

private:
    inline static jfieldID art_method_field = nullptr;
    inline static size_t art_method_size = 0;
    inline static size_t entry_point_offset = 0;
    inline static size_t interpreter_entry_point_offset = 0;
    inline static size_t data_offset = 0;
    inline static size_t access_flags_offset = 0;
    inline static size_t declaring_class_offset = std::numeric_limits<size_t>::max();
    inline static uint32_t kAccFastInterpreterToInterpreterInvoke = 0x40000000;
    inline static uint32_t kAccPreCompiled = 0x00200000;
    inline static uint32_t kAccCompileDontBother = 0x02000000;
    inline static uint32_t kAccDefaultConflict = 0x01000000;
    inline static uint32_t kAccIntrinsic = 0x80000000;
};

}  // namespace lsplant::art
