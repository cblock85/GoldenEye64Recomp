// Live game-code loader: builds the entire recompiled game at runtime from the
// user's ROM using N64Recomp's live recompiler. This lets distributable builds
// ship with ZERO game-derived code — only symbol facts (names/addresses/sizes)
// and weak forwarder stubs are compiled in; the executable machine code for all
// game functions is generated in memory after the user's ROM is loaded.
//
// Dispatch model: the recompiler context is configured with
// use_lookup_for_all_function_calls, so every jal/tail-call in the generated
// code resolves through live_resolver() below. That one choke point routes:
//   - RECOMP_PATCH'd functions  -> their statically compiled patch (our code)
//   - libultra reimplementations -> librecomp's shims (via get_function tables)
//   - everything else            -> the live-recompiled machine code
// The weak forwarders in LiveGamecode/forwarders.c bring calls from app code
// (patches, entrypoint, overlay tables) into the same dispatch.

#ifdef GE_LIVE_GAMECODE

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <vector>
#include <filesystem>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include "recompiler/context.h"
#include "recompiler/live_recompiler.h"
#include "librecomp/game.hpp"
#include "common/RabbitizerConfig.h"

#include "ge_dump_toml.h"

// librecomp/ultramodern runtime hooks (all extern "C" in their homes).
extern "C" {
    recomp_func_t* get_function(int32_t vram);
    void cop0_status_write(recomp_context* ctx, gpr value);
    gpr cop0_status_read(recomp_context* ctx);
    void switch_error(const char* func, uint32_t vram, uint32_t jtbl);
    void do_break(uint32_t vram);
    void pause_self(uint8_t* rdram);
}

struct LiveOverride {
    uint32_t vram;
    recomp_func_t* func;
};

#include "../../LiveGamecode/live_overrides.inl"

namespace {

// Keep in sync with us.toml ([patches] stubs / ignored).
const char* const config_stubbed[] = {
    "tlbRandomGetNext", "resolve_TLBaddress_for_InvalidHit", "initTLBPrepareContext",
};
const char* const config_ignored[] = {
    "next_interrupt", "IP6_Hdlr", "IP7_Hdlr", "counter", "cart", "rcp", "prenmi",
    "sw2", "swl", "redispatch", "enqueueRunning", "__osDispatchThreadSave",
};

struct SectionTable {
    uint32_t base = 0;
    uint32_t size = 0;
    std::vector<recomp_func_t*> funcs; // indexed by (vram - base) / 4
};

std::once_flag load_once;
bool load_ok = false;
std::unique_ptr<N64Recomp::LiveGeneratorOutput> live_output;
std::vector<SectionTable> live_tables;
std::unordered_map<uint32_t, recomp_func_t*> override_map;
std::vector<int32_t> section_addresses;

recomp_func_t* live_lookup_raw(uint32_t vram) {
    for (auto& tbl : live_tables) {
        if (vram >= tbl.base && vram < tbl.base + tbl.size) {
            recomp_func_t* f = tbl.funcs[(vram - tbl.base) / 4];
            if (f != nullptr) {
                return f;
            }
        }
    }
    return nullptr;
}

// The game's cosf falls through into sinf (cos(x) = sin(x + pi/2)); the
// recompiler truncates it at the boundary, so implement it here.
void cosf_fallthrough_shim(uint8_t* rdram, recomp_context* ctx) {
    ctx->f12.fl += 1.57079632679489661923f;
    recomp_func_t* sinf_func = live_lookup_raw(GE_SINF_VRAM);
    if (sinf_func != nullptr) {
        sinf_func(rdram, ctx);
    }
}

// Runtime function resolver used for ALL calls inside the live-recompiled code.
recomp_func_t* live_resolver(int32_t vram_signed) {
    uint32_t vram = static_cast<uint32_t>(vram_signed);


    auto ov = override_map.find(vram);
    if (ov != override_map.end()) {
        return ov->second;
    }

    if (recomp_func_t* f = live_lookup_raw(vram)) {
        return f;
    }

    // librecomp's tables: libultra shims and anything registered statically.
    return get_function(vram_signed);
}

bool build_live_gamecode() {
    std::vector<uint8_t> rom;
    if (const char* rom_env = getenv("GE_LIVE_ROM")) {
        std::ifstream rf{rom_env, std::ios::binary};
        rom.assign(std::istreambuf_iterator<char>(rf), {});
    }
    else {
        std::span<const uint8_t> rom_span = recomp::get_rom();
        rom.assign(rom_span.begin(), rom_span.end());
    }
    if (rom.empty()) {
        fprintf(stderr, "live_gamecode: no ROM loaded\n");
        return false;
    }

    // Write the embedded symbol context to a temp file for the parser.
    std::filesystem::path dump_path = std::filesystem::temp_directory_path() / "ge_dump_context.toml";
    {
        std::ofstream dump_out{dump_path, std::ios::binary};
        dump_out.write(ge_dump_toml, sizeof(ge_dump_toml));
    }

    N64Recomp::Context context{};
    if (!N64Recomp::Context::from_symbol_file(dump_path, std::move(rom), context, true)) {
        fprintf(stderr, "live_gamecode: failed to parse symbol context\n");
        return false;
    }

    // Apply the same built-in renames/flags the N64Recomp CLI applies in symbols mode.
    auto rename_function = [&context](size_t func_index, const std::string& new_name) {
        N64Recomp::Function& func = context.functions[func_index];
        context.functions_by_name.erase(func.name);
        func.name = new_name;
        context.functions_by_name[func.name] = func_index;
    };
    for (size_t i = 0; i < context.functions.size(); i++) {
        N64Recomp::Function& func = context.functions[i];
        if (N64Recomp::reimplemented_funcs.contains(func.name)) {
            rename_function(i, func.name + "_recomp");
            func.reimplemented = true;
            func.ignored = true;
        } else if (N64Recomp::ignored_funcs.contains(func.name)) {
            rename_function(i, func.name + "_recomp");
            func.ignored = true;
        } else if (N64Recomp::renamed_funcs.contains(func.name)) {
            rename_function(i, func.name + "_recomp");
            func.ignored = false;
        }
    }

    // Config-level stubs and ignores (kept in sync with us.toml).
    for (const char* name : config_stubbed) {
        auto it = context.functions_by_name.find(name);
        if (it != context.functions_by_name.end()) {
            context.functions[it->second].stubbed = true;
        }
    }
    for (const char* name : config_ignored) {
        auto it = context.functions_by_name.find(name);
        if (it != context.functions_by_name.end()) {
            context.functions[it->second].ignored = true;
        }
    }

    // Route every call through the runtime resolver.
    context.use_lookup_for_all_function_calls = true;

    // Match the N64Recomp CLI's disassembler configuration (no pseudo-instructions).
    RabbitizerConfig_Cfg.pseudos.pseudoMove = false;
    RabbitizerConfig_Cfg.pseudos.pseudoBeqz = false;
    RabbitizerConfig_Cfg.pseudos.pseudoBnez = false;
    RabbitizerConfig_Cfg.pseudos.pseudoNot = false;
    RabbitizerConfig_Cfg.pseudos.pseudoBal = false;

    section_addresses.resize(context.sections.size());
    for (size_t i = 0; i < context.sections.size(); i++) {
        section_addresses[i] = static_cast<int32_t>(context.sections[i].ram_addr);
    }

    N64Recomp::LiveGeneratorInputs inputs{
        .base_event_index = 0,
        .cop0_status_write = cop0_status_write,
        .cop0_status_read = cop0_status_read,
        .switch_error = switch_error,
        .do_break = do_break,
        .get_function = live_resolver,
        .syscall_handler = nullptr,
        .pause_self = pause_self,
        .trigger_event = nullptr,
        .reference_section_addresses = nullptr,
        .local_section_addresses = section_addresses.data(),
        .run_hook = nullptr,
    };

    N64Recomp::LiveGenerator generator{context.functions.size(), inputs};
    std::vector<std::vector<uint32_t>> static_funcs{context.sections.size()};

    size_t recompiled = 0;
    for (size_t i = 0; i < context.functions.size(); i++) {
        const auto& func = context.functions[i];
        if (func.ignored || func.words.empty()) {
            continue;
        }
        if (getenv("GE_LIVE_DEBUG")) {
            fprintf(stderr, "[live] %zu %s @%08X size %zu\n", i, func.name.c_str(), func.vram, func.words.size() * 4);
            fflush(stderr);
        }
        std::ostringstream dummy{};
        if (!N64Recomp::recompile_function_live(generator, context, i, dummy, static_funcs, false)) {
            fprintf(stderr, "live_gamecode: failed to recompile %s\n", func.name.c_str());
            return false;
        }
        recompiled++;
    }

    for (const auto& statics : static_funcs) {
        if (!statics.empty()) {
            fprintf(stderr, "live_gamecode: unexpected undeclared static functions found\n");
            return false;
        }
    }

    live_output = std::make_unique<N64Recomp::LiveGeneratorOutput>(generator.finish());
    if (!live_output->good) {
        fprintf(stderr, "live_gamecode: code generation failed\n");
        return false;
    }

    // Build per-section dispatch tables.
    live_tables.clear();
    live_tables.resize(context.sections.size());
    for (size_t i = 0; i < context.sections.size(); i++) {
        live_tables[i].base = context.sections[i].ram_addr;
        live_tables[i].size = static_cast<uint32_t>(context.sections[i].size);
        live_tables[i].funcs.assign((live_tables[i].size + 3) / 4, nullptr);
    }
    for (size_t i = 0; i < context.functions.size(); i++) {
        const auto& func = context.functions[i];
        if (func.ignored || func.words.empty()) {
            continue;
        }
        recomp_func_t* generated = live_output->functions[i];
        if (generated != nullptr) {
            auto& tbl = live_tables[func.section_index];
            tbl.funcs[(func.vram - tbl.base) / 4] = generated;
        }
    }

    // librecomp-provided libultra shims first (weak decls; only non-null ones exist),
    // then patch overrides (statically compiled RECOMP_PATCH functions), then the cosf shim.
    override_map.clear();
    for (const LiveOverride& ov : live_shim_list) {
        if (ov.func != nullptr) {
            override_map[ov.vram] = ov.func;
        }
    }
#ifndef _WIN32



    // Hybrid debug mode: route a vram range to the statically recompiled functions
    // from a dlopen'd library, to bisect live-recompiler correctness issues.
    if (const char* hybrid_so = getenv("GE_LIVE_HYBRID")) {
        uint32_t lo = getenv("GE_HYB_LO") ? strtoul(getenv("GE_HYB_LO"), nullptr, 16) : 0u;
        uint32_t hi = getenv("GE_HYB_HI") ? strtoul(getenv("GE_HYB_HI"), nullptr, 16) : 0xFFFFFFFFu;
        void* handle = dlopen(hybrid_so, RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            fprintf(stderr, "hybrid: dlopen failed: %s\n", dlerror());
        }
        else {
            size_t routed = 0;
            for (const auto& func : context.functions) {
                if (func.ignored || func.words.empty() || func.vram < lo || func.vram >= hi) {
                    continue;
                }
                if (void* sym = dlsym(handle, func.name.c_str())) {
                    override_map[func.vram] = reinterpret_cast<recomp_func_t*>(sym);
                    routed++;
                }
            }
            fprintf(stderr, "hybrid: routed %zu functions [%08X..%08X) to static code\n", routed, lo, hi);
        }
    }
#endif

    for (const LiveOverride& ov : live_override_list) {
        override_map[ov.vram] = ov.func;
    }
    override_map[GE_COSF_VRAM] = cosf_fallthrough_shim;

    std::error_code ec;
    std::filesystem::remove(dump_path, ec);

    printf("live_gamecode: recompiled %zu functions from ROM at runtime\n", recompiled);
    return true;
}

} // anonymous namespace

// Debug/selftest: build the live code immediately (main thread) and report success.
extern "C" bool live_gamecode_selftest() {
    std::call_once(load_once, []() { load_ok = build_live_gamecode(); });
    return load_ok;
}

// Called by the weak forwarders for every game function invocation from app code.
extern "C" void live_call(unsigned int vram, uint8_t* rdram, recomp_context* ctx) {
    std::call_once(load_once, []() { load_ok = build_live_gamecode(); });

    if (!load_ok) {
        fprintf(stderr, "live_gamecode: game code unavailable (vram 0x%08X)\n", vram);
        abort();
    }


    auto ov = override_map.find(vram);
    if (ov != override_map.end() && ov->second != nullptr) {
        // Overrides are strong symbols that already shadow the weak forwarders,
        // so reaching here with an override means the caller looked us up by
        // address; dispatch to the override.
        ov->second(rdram, ctx);
        return;
    }

    recomp_func_t* f = live_lookup_raw(vram);
    if (f == nullptr) {
        fprintf(stderr, "live_gamecode: no generated code for vram 0x%08X\n", vram);
        abort();
    }
    f(rdram, ctx);
}

#endif // GE_LIVE_GAMECODE
