
#include "callconv.h"
#include "codegenerator.h"
#include "config.h"
#include "connection.h"
#include "instrew-server-config.h"
#include "optimizer.h"
#include "version.h"

#include <rellume/rellume.h>

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassTimingInfo.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <openssl/sha.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <unordered_map>


namespace {

enum class DumpIR {
    Lift, CC, Opt, CodeGen,
};

llvm::cl::opt<bool> enableProfiling("profile", llvm::cl::desc("Profile translation"), llvm::cl::cat(InstrewCategory));
llvm::cl::opt<bool> enableTracing("trace", llvm::cl::desc("Trace execution (lots of logs)"), llvm::cl::cat(InstrewCategory));
llvm::cl::opt<unsigned char> perfSupport("perf", llvm::cl::desc("Enable perf support:"),
    llvm::cl::values(
        clEnumVal(0, "disabled"),
        clEnumVal(1, "write perf memory map"),
        clEnumVal(2, "write jitdump file")
        ),
    llvm::cl::cat(InstrewCategory));
llvm::cl::opt<bool> verifyLiftedIR("verify-lifted", llvm::cl::desc("Verify lifted IR"), llvm::cl::cat(InstrewCategory));
llvm::cl::bits<DumpIR> dumpIR("dumpir", llvm::cl::desc("Dump IR after:"),
    llvm::cl::values(
        clEnumValN(DumpIR::Lift, "lift", "lifting"),
        clEnumValN(DumpIR::CC, "cc", "calling convention"),
        clEnumValN(DumpIR::Opt, "opt", "optimizations"),
        clEnumValN(DumpIR::CodeGen, "codegen", "code generation")
        ), llvm::cl::cat(InstrewCategory));
llvm::cl::opt<bool> safeCallRet("safe-call-ret", llvm::cl::desc("Don't clobber flags on call/ret instructions"), llvm::cl::cat(CodeGenCategory));
llvm::cl::opt<bool> enableCallret("callret", llvm::cl::desc("Enable call-ret lifting"), llvm::cl::cat(CodeGenCategory));
llvm::cl::opt<bool> enableFastcc("fastcc", llvm::cl::desc("Enable register-based calling convention (default: true)"), llvm::cl::init(true), llvm::cl::cat(CodeGenCategory));
llvm::cl::opt<bool> enablePIC("pic", llvm::cl::desc("Compile code position-independent"), llvm::cl::cat(CodeGenCategory));

} // end anonymous namespace

#define SPTR_ADDR_SPACE 1

static llvm::Function* CreateFunc(llvm::LLVMContext& ctx,
                                  const std::string name) {
    llvm::Type* sptr = llvm::PointerType::get(ctx, SPTR_ADDR_SPACE);
    llvm::Type* void_ty = llvm::Type::getVoidTy(ctx);
    auto* fn_ty = llvm::FunctionType::get(void_ty, {sptr}, false);
    auto linkage = llvm::GlobalValue::ExternalLinkage;
    return llvm::Function::Create(fn_ty, linkage, name);
}

static llvm::GlobalVariable* CreatePcBase(llvm::LLVMContext& ctx) {
    llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
    auto* pc_base_var = new llvm::GlobalVariable(i64, false,
                                                 llvm::GlobalValue::ExternalLinkage);
    pc_base_var->setName("instrew_baseaddr");
    llvm::Constant* lim_val = llvm::ConstantInt::get(i64, -1);
    llvm::Metadata* lim = llvm::ConstantAsMetadata::get(lim_val);
    llvm::MDNode* node = llvm::MDNode::get(ctx, {lim, lim});
    pc_base_var->setMetadata("absolute_symbol", node);
    return pc_base_var;
}

struct IWState {
private:
    IWConnection* iwc;
    const IWServerConfig* iwsc = nullptr;
    IWClientConfig* iwcc = nullptr;
    CallConv instrew_cc = CallConv::CDECL;

    LLConfig* rlcfg;
    llvm::LLVMContext ctx;
    llvm::Constant* pc_base;
    llvm::SmallVector<llvm::Function*, 8> helper_fns;
    std::unique_ptr<llvm::Module> mod;

    Optimizer optimizer;
    llvm::SmallVector<char, 4096> obj_buffer;
    CodeGenerator codegen;

    llvm::SmallVector<uint8_t, 256> hashBuffer;

    std::chrono::steady_clock::duration dur_predecode{};
    std::chrono::steady_clock::duration dur_lifting{};
    std::chrono::steady_clock::duration dur_instrument{};
    std::chrono::steady_clock::duration dur_llvm_opt{};
    std::chrono::steady_clock::duration dur_llvm_codegen{};

    void appendConfig(llvm::SmallVectorImpl<uint8_t>& buffer) const {
        struct {
            uint32_t version = 2;
            uint8_t safeCallRet = safeCallRet;
            uint8_t enableCallret = enableCallret;
            uint8_t enableFastcc = enableFastcc;
            uint8_t enablePIC = enablePIC;

            uint32_t guestArch;
            uint32_t hostArch;
            uint32_t stackAlignment;
        } config;
        config.guestArch = iwsc->tsc_guest_arch;
        config.hostArch = iwsc->tsc_host_arch;
        config.stackAlignment = iwsc->tsc_stack_alignment;

        std::size_t start = buffer.size();
        buffer.resize_for_overwrite(buffer.size() + sizeof(config));
        std::memcpy(&buffer[start], &config, sizeof(config));
    }

public:

    IWState(IWConnection* iwc)
            : codegen(*iw_get_sc(iwc), enablePIC, obj_buffer) {
        this->iwc = iwc;
        iwsc = iw_get_sc(iwc);
        iwcc = iw_get_cc(iwc);

#ifndef NDEBUG
        // Discard non-global value names in release builds.
        ctx.setDiscardValueNames(true);
#endif

        rlcfg = ll_config_new();
        ll_config_enable_verify_ir(rlcfg, verifyLiftedIR);
        ll_config_set_call_ret_clobber_flags(rlcfg, !safeCallRet);
        ll_config_set_sptr_addrspace(rlcfg, SPTR_ADDR_SPACE);
        ll_config_enable_overflow_intrinsics(rlcfg, false);
        if (enableCallret) {
            auto call_fn = CreateFunc(ctx, "instrew_call_cdecl");
            helper_fns.push_back(call_fn);
            ll_config_set_tail_func(rlcfg, llvm::wrap(call_fn));
            ll_config_set_call_func(rlcfg, llvm::wrap(call_fn));
        }
        if (iwsc->tsc_guest_arch == EM_X86_64) {
            ll_config_set_architecture(rlcfg, "x86-64");

            auto syscall_fn = CreateFunc(ctx, "syscall");
            helper_fns.push_back(syscall_fn);
            ll_config_set_syscall_impl(rlcfg, llvm::wrap(syscall_fn));

            // cpuinfo function is CPUID on x86-64.
            llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);
            llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
            auto i64_i64 = llvm::StructType::get(i64, i64);
            auto cpuinfo_fn_ty = llvm::FunctionType::get(i64_i64, {i32, i32}, false);
            auto linkage = llvm::GlobalValue::ExternalLinkage;
            auto cpuinfo_fn = llvm::Function::Create(cpuinfo_fn_ty, linkage, "cpuid");
            helper_fns.push_back(cpuinfo_fn);
            ll_config_set_cpuinfo_func(rlcfg, llvm::wrap(cpuinfo_fn));
        } else if (iwsc->tsc_guest_arch == EM_RISCV) {
            ll_config_set_architecture(rlcfg, "rv64");

            auto syscall_fn = CreateFunc(ctx, "syscall_rv64");
            helper_fns.push_back(syscall_fn);
            ll_config_set_syscall_impl(rlcfg, llvm::wrap(syscall_fn));
        } else if (iwsc->tsc_guest_arch == EM_AARCH64) {
            ll_config_set_architecture(rlcfg, "aarch64");

            auto syscall_fn = CreateFunc(ctx, "syscall_aarch64");
            helper_fns.push_back(syscall_fn);
            ll_config_set_syscall_impl(rlcfg, llvm::wrap(syscall_fn));
        } else {
            std::cerr << "error: unsupported architecture" << std::endl;
            abort();
        }

        // Backward compatibility -- only one fast CC per guest--host pair now.
        if (enableFastcc)
            instrew_cc = GetFastCC(iwsc->tsc_host_arch, iwsc->tsc_guest_arch);
        else
            instrew_cc = CallConv::CDECL;
        iwcc->tc_callconv = GetCallConvClientNumber(instrew_cc);
        iwcc->tc_profile = enableProfiling;
        iwcc->tc_perf = perfSupport;
        iwcc->tc_print_trace = enableTracing;

        llvm::GlobalVariable* pc_base_var = CreatePcBase(ctx);
        pc_base = llvm::ConstantExpr::getPtrToInt(pc_base_var,
                                                  llvm::Type::getInt64Ty(ctx));

        mod = std::make_unique<llvm::Module>("mod", ctx);
#if LL_LLVM_MAJOR >= 13
        if (iwsc->tsc_stack_alignment != 0)
            mod->setOverrideStackAlignment(iwsc->tsc_stack_alignment);
#endif
#if LL_LLVM_MAJOR >= 19
        mod->setIsNewDbgInfoFormat(true);
#endif

#if LL_LLVM_MAJOR < 17
        mod->getGlobalList().push_back(pc_base_var);
#else
        mod->insertGlobalVariable(pc_base_var);
#endif
        for (const auto& helper_fn : helper_fns)
            mod->getFunctionList().push_back(helper_fn);

        llvm::SmallVector<llvm::Constant*, 8> used;
        used.push_back(pc_base_var);

        for (llvm::Function& fn : mod->functions())
            used.push_back(&fn);

        llvm::Type* ptrTy = llvm::PointerType::get(ctx, 0);
        llvm::ArrayType* used_ty = llvm::ArrayType::get(ptrTy, used.size());
        llvm::GlobalVariable* llvm_used = new llvm::GlobalVariable(
                *mod, used_ty, /*const=*/false,
                llvm::GlobalValue::AppendingLinkage,
                llvm::ConstantArray::get(used_ty, used), "llvm.used");
        llvm_used->setSection("llvm.metadata");

        codegen.GenerateCode(mod.get());
        iw_sendobj(iwc, 0, obj_buffer.data(), obj_buffer.size(), nullptr);

        for (llvm::Function& fn : mod->functions())
            if (fn.hasExternalLinkage() && !fn.empty())
                fn.deleteBody();

        appendConfig(hashBuffer);
        optimizer.appendConfig(hashBuffer);
        codegen.appendConfig(hashBuffer);
    }
    ~IWState() {
        if (enableProfiling) {
            std::cerr << "Server profile: " << std::dec
                      << std::chrono::duration_cast<std::chrono::milliseconds>(dur_predecode).count()
                      << "ms predecode; "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(dur_lifting).count()
                      << "ms lifting; "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(dur_instrument).count()
                      << "ms instrumentation; "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(dur_llvm_opt).count()
                      << "ms llvm_opt; "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(dur_llvm_codegen).count()
                      << "ms llvm_codegen"
                      << std::endl;
        }
        llvm::reportAndResetTimings(&llvm::errs());
        ll_config_free(rlcfg);
    }

    void Translate(uintptr_t addr) {
        auto time_predecode_start = std::chrono::steady_clock::now();

        // Optionally generate position-independent code, where the offset
        // can be adjusted using relocations.
        if (enablePIC)
            ll_config_set_pc_base(rlcfg, addr, llvm::wrap(pc_base));

        LLFunc* rlfn = ll_func_new(llvm::wrap(mod.get()), rlcfg);
        RellumeMemAccessCb accesscb = [](size_t addr, uint8_t* buf, size_t bufsz, void* user_arg) {
            auto iwc = reinterpret_cast<IWConnection*>(user_arg);
            return iw_readmem(iwc, addr, addr + bufsz, buf);
        };
        int fail = ll_func_decode_cfg(rlfn, addr, accesscb, reinterpret_cast<void*>(iwc));
        if (fail) {
            std::cerr << "error: decode failed 0x" << std::hex << addr << std::endl;
            ll_func_dispose(rlfn);
            iw_sendobj(iwc, addr, nullptr, 0, nullptr);
            return;
        }

        size_t hashConfigEnd = hashBuffer.size();

        // Store address only for non-PIC code, other addresses are relative.
        uint64_t hashAddr = enablePIC ? 0 : addr;
        hashBuffer.resize_for_overwrite(hashConfigEnd + sizeof(hashAddr));
        memcpy(&hashBuffer[hashConfigEnd], &hashAddr, sizeof(hashAddr));

        const struct RellumeCodeRange* ranges = ll_func_ranges(rlfn);
        for (; ranges->start || ranges->end; ranges++) {
            uint64_t rel_start = ranges->start - addr;
            uint64_t size = ranges->end - ranges->start;

            size_t bufEnd = hashBuffer.size();
            hashBuffer.resize_for_overwrite(bufEnd + 2 * sizeof(uint64_t) + size);
            memcpy(&hashBuffer[bufEnd], &rel_start, sizeof(uint64_t));
            memcpy(&hashBuffer[bufEnd + sizeof(uint64_t)], &size, sizeof(uint64_t));
            iw_readmem(iwc, ranges->start, ranges->end, &hashBuffer[bufEnd + 2 * sizeof(uint64_t)]);
        }

        uint8_t hash[SHA_DIGEST_LENGTH];
        SHA1(hashBuffer.data(), hashBuffer.size(), hash);
        hashBuffer.truncate(hashConfigEnd);

        if (iw_cache_probe(iwc, addr, hash)) {
            ll_func_dispose(rlfn);
            if (enableProfiling)
                dur_predecode += std::chrono::steady_clock::now() - time_predecode_start;
            return;
        }

        auto time_lifting_start = std::chrono::steady_clock::now();
        LLVMValueRef fn_wrapped = !fail ? ll_func_lift(rlfn) : nullptr;
        if (!fn_wrapped) {
            std::cerr << "error: lift failed 0x" << std::hex << addr << "\n";
            ll_func_dispose(rlfn);
            iw_sendobj(iwc, addr, nullptr, 0, nullptr);
            return;
        }

        llvm::Function* fn = llvm::unwrap<llvm::Function>(fn_wrapped);
        fn->setName("S0_" + llvm::Twine::utohexstr(addr));
        ll_func_dispose(rlfn);

        if (dumpIR.isSet(DumpIR::Lift))
            mod->print(llvm::errs(), nullptr);

        auto time_instrument_start = std::chrono::steady_clock::now();
        fn = ChangeCallConv(fn, instrew_cc);
        if (dumpIR.isSet(DumpIR::CC))
            mod->print(llvm::errs(), nullptr);

        auto time_llvm_opt_start = std::chrono::steady_clock::now();
        optimizer.Optimize(fn);
        if (dumpIR.isSet(DumpIR::Opt))
            mod->print(llvm::errs(), nullptr);

        auto time_llvm_codegen_start = std::chrono::steady_clock::now();
        codegen.GenerateCode(mod.get());
        if (dumpIR.isSet(DumpIR::CodeGen))
            mod->print(llvm::errs(), nullptr);

        iw_sendobj(iwc, addr, obj_buffer.data(), obj_buffer.size(), hash);

        // Remove unused functions and dead prototypes. Having many prototypes
        // causes some compile-time overhead.
        for (auto& glob_fn : llvm::make_early_inc_range(*mod))
            if (glob_fn.use_empty())
                glob_fn.eraseFromParent();

        if (enableProfiling) {
            dur_predecode += time_lifting_start - time_predecode_start;
            dur_lifting += time_instrument_start - time_lifting_start;
            dur_instrument += time_llvm_opt_start - time_instrument_start;
            dur_llvm_opt += time_llvm_codegen_start - time_llvm_opt_start;
            dur_llvm_codegen += std::chrono::steady_clock::now() - time_llvm_codegen_start;
        }
    }
};


int main(int argc, char** argv) {
    llvm::cl::HideUnrelatedOptions({&InstrewCategory, &CodeGenCategory});
    auto& optionMap = llvm::cl::getRegisteredOptions();
    optionMap["time-passes"]->setHiddenFlag(llvm::cl::Hidden);
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream& os) {
        os << "Instrew " << instrew::instrewVersion << "\n";
    });
    llvm::cl::ParseCommandLineOptions(argc, argv);

    static const IWFunctions iwf = {
        /*.init=*/[](IWConnection* iwc) {
            return new IWState(iwc);
        },
        /*.translate=*/[](IWState* state, uintptr_t addr) {
            state->Translate(addr);
        },
        /*.finalize=*/[](IWState* state) {
            delete state;
        },
    };

    return iw_run_server(&iwf, argc, argv);
}
