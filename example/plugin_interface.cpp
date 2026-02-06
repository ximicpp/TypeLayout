// Boost.TypeLayout (v2.0)
//
// Plugin/DLL Interface Verification Example
//
// This example demonstrates TypeLayout's ability to detect ABI mismatches
// when loading plugins or dynamic libraries at runtime.
//
// Uses Layout layer for byte-level compatibility verification.
// (For stricter ABI checking, consider Definition layer hashes which
// also verify field names and inheritance structure.)
//
// Problem Solved:
//   When a host application loads a plugin (DLL/SO), they must agree on
//   the interface struct layouts. If the plugin was compiled with different:
//   - Struct definitions (added/removed/reordered fields)
//   - Compiler settings (packing, alignment)
//   - Compiler versions (different ABI)
//   
//   Traditional approach: Silent data corruption or mysterious crashes
//   With TypeLayout: Immediate detection at plugin load time
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
#include <memory>

using namespace boost::typelayout;

// =============================================================================
// Plugin Interface Definition (shared between host and plugins)
// =============================================================================

/// Plugin metadata - describes the plugin capabilities
struct PluginInfo {
    char name[64];
    char version[16];
    uint32_t api_version;
    uint64_t capabilities;  // Bitmask of capabilities
};

/// Audio processing context - passed to plugin for processing
struct AudioContext {
    float* input_buffer;
    float* output_buffer;
    uint32_t buffer_size;
    uint32_t sample_rate;
    uint32_t num_channels;
    double tempo;           // BPM
    double position;        // Playback position in seconds
};

/// Plugin parameter for automation
struct PluginParameter {
    char name[32];
    float value;
    float min_value;
    float max_value;
    uint32_t flags;         // Parameter flags (automatable, etc.)
};

// Verify all interface types are supported
static_assert(LayoutSupported<PluginInfo>, "PluginInfo must be layout-supported");
static_assert(LayoutSupported<AudioContext>, "AudioContext must be layout-supported");
static_assert(LayoutSupported<PluginParameter>, "PluginParameter must be layout-supported");

// =============================================================================
// Plugin Interface Contract with Layout Verification
// =============================================================================

/// Interface contract that plugins must implement
/// Contains layout hashes for runtime verification
struct PluginInterface {
    // Layout hashes for verification (computed at compile-time)
    uint64_t plugin_info_hash;
    uint64_t audio_context_hash;
    uint64_t plugin_parameter_hash;
    
    // Function pointers (plugin implementation)
    void (*get_info)(PluginInfo* info);
    void (*process)(AudioContext* ctx);
    int  (*get_parameter_count)();
    void (*get_parameter)(int index, PluginParameter* param);
    void (*set_parameter)(int index, float value);
    
    // Initialization
    void (*initialize)(uint32_t sample_rate);
    void (*shutdown)();
};

// Expected hashes (host's view of the interface)
constexpr uint64_t EXPECTED_PLUGIN_INFO_HASH = get_layout_hash<PluginInfo>();
constexpr uint64_t EXPECTED_AUDIO_CONTEXT_HASH = get_layout_hash<AudioContext>();
constexpr uint64_t EXPECTED_PLUGIN_PARAMETER_HASH = get_layout_hash<PluginParameter>();

// =============================================================================
// Host Application - Plugin Loader with Verification
// =============================================================================

enum class LoadResult {
    OK,
    PLUGIN_INFO_MISMATCH,
    AUDIO_CONTEXT_MISMATCH,
    PLUGIN_PARAMETER_MISMATCH,
    NULL_INTERFACE
};

const char* load_result_str(LoadResult r) {
    switch (r) {
        case LoadResult::OK: return "OK";
        case LoadResult::PLUGIN_INFO_MISMATCH: return "PluginInfo layout mismatch";
        case LoadResult::AUDIO_CONTEXT_MISMATCH: return "AudioContext layout mismatch";
        case LoadResult::PLUGIN_PARAMETER_MISMATCH: return "PluginParameter layout mismatch";
        case LoadResult::NULL_INTERFACE: return "Null interface pointer";
    }
    return "Unknown";
}

/// Verify plugin interface compatibility before use
LoadResult verify_plugin_interface(const PluginInterface* iface) {
    if (!iface) {
        return LoadResult::NULL_INTERFACE;
    }
    
    // Verify each struct layout matches
    if (iface->plugin_info_hash != EXPECTED_PLUGIN_INFO_HASH) {
        std::cerr << "  PluginInfo hash mismatch!\n";
        std::cerr << "    Plugin has:  0x" << std::hex << iface->plugin_info_hash << "\n";
        std::cerr << "    Host expects: 0x" << EXPECTED_PLUGIN_INFO_HASH << std::dec << "\n";
        return LoadResult::PLUGIN_INFO_MISMATCH;
    }
    
    if (iface->audio_context_hash != EXPECTED_AUDIO_CONTEXT_HASH) {
        std::cerr << "  AudioContext hash mismatch!\n";
        std::cerr << "    Plugin has:  0x" << std::hex << iface->audio_context_hash << "\n";
        std::cerr << "    Host expects: 0x" << EXPECTED_AUDIO_CONTEXT_HASH << std::dec << "\n";
        return LoadResult::AUDIO_CONTEXT_MISMATCH;
    }
    
    if (iface->plugin_parameter_hash != EXPECTED_PLUGIN_PARAMETER_HASH) {
        std::cerr << "  PluginParameter hash mismatch!\n";
        std::cerr << "    Plugin has:  0x" << std::hex << iface->plugin_parameter_hash << "\n";
        std::cerr << "    Host expects: 0x" << EXPECTED_PLUGIN_PARAMETER_HASH << std::dec << "\n";
        return LoadResult::PLUGIN_PARAMETER_MISMATCH;
    }
    
    return LoadResult::OK;
}

/// Simulated plugin loading (in real code: dlopen/LoadLibrary)
class PluginHost {
public:
    bool load_plugin(const PluginInterface* iface, const char* plugin_path) {
        std::cout << "[Host] Loading plugin: " << plugin_path << "\n";
        
        // CRITICAL: Verify layout compatibility BEFORE using the interface
        auto result = verify_plugin_interface(iface);
        if (result != LoadResult::OK) {
            std::cerr << "[Host] Plugin verification FAILED: " << load_result_str(result) << "\n";
            std::cerr << "[Host] The plugin was compiled with incompatible struct definitions.\n";
            std::cerr << "[Host] This could be due to:\n";
            std::cerr << "  - Different plugin API version\n";
            std::cerr << "  - Different compiler or compiler settings\n";
            std::cerr << "  - Modified struct definitions\n";
            return false;
        }
        
        std::cout << "[Host] Plugin verification PASSED!\n";
        
        // Safe to use the plugin now
        plugin_ = iface;
        
        // Get plugin info
        PluginInfo info{};
        plugin_->get_info(&info);
        std::cout << "[Host] Loaded: " << info.name << " v" << info.version << "\n";
        
        // Initialize plugin
        plugin_->initialize(44100);
        
        return true;
    }
    
    void process_audio(float* input, float* output, uint32_t size) {
        if (!plugin_) return;
        
        AudioContext ctx{};
        ctx.input_buffer = input;
        ctx.output_buffer = output;
        ctx.buffer_size = size;
        ctx.sample_rate = 44100;
        ctx.num_channels = 2;
        ctx.tempo = 120.0;
        ctx.position = 0.0;
        
        plugin_->process(&ctx);
    }
    
    void unload() {
        if (plugin_) {
            plugin_->shutdown();
            plugin_ = nullptr;
        }
    }
    
private:
    const PluginInterface* plugin_ = nullptr;
};

// =============================================================================
// Simulated Compatible Plugin (correct interface)
// =============================================================================

namespace compatible_plugin {

void get_info(PluginInfo* info) {
    std::strcpy(info->name, "Compatible Audio Filter");
    std::strcpy(info->version, "1.0.0");
    info->api_version = 1;
    info->capabilities = 0x01;  // Basic processing
}

void process(AudioContext* ctx) {
    // Simple pass-through with gain
    for (uint32_t i = 0; i < ctx->buffer_size * ctx->num_channels; ++i) {
        ctx->output_buffer[i] = ctx->input_buffer[i] * 0.8f;
    }
}

int get_parameter_count() { return 1; }

void get_parameter(int index, PluginParameter* param) {
    if (index == 0) {
        std::strcpy(param->name, "Gain");
        param->value = 0.8f;
        param->min_value = 0.0f;
        param->max_value = 1.0f;
        param->flags = 1;  // Automatable
    }
}

void set_parameter(int, float) {}
void initialize(uint32_t) { std::cout << "[Plugin] Initialized\n"; }
void shutdown() { std::cout << "[Plugin] Shutdown\n"; }

// Create interface with CORRECT hashes
PluginInterface create_interface() {
    return PluginInterface{
        .plugin_info_hash = get_layout_hash<PluginInfo>(),
        .audio_context_hash = get_layout_hash<AudioContext>(),
        .plugin_parameter_hash = get_layout_hash<PluginParameter>(),
        .get_info = get_info,
        .process = process,
        .get_parameter_count = get_parameter_count,
        .get_parameter = get_parameter,
        .set_parameter = set_parameter,
        .initialize = initialize,
        .shutdown = shutdown
    };
}

} // namespace compatible_plugin

// =============================================================================
// Simulated INCOMPATIBLE Plugin (different struct definition)
// =============================================================================

namespace incompatible_plugin {

// This plugin was compiled with a DIFFERENT version of PluginInfo!
// (Simulating what happens when struct definitions change between versions)
struct PluginInfoV2 {
    char name[64];
    char version[16];
    char author[32];        // NEW FIELD - breaks compatibility!
    uint32_t api_version;
    uint64_t capabilities;
};

// This plugin was compiled with a DIFFERENT AudioContext!
struct AudioContextV2 {
    float* input_buffer;
    float* output_buffer;
    uint32_t buffer_size;
    uint32_t sample_rate;
    uint32_t num_channels;
    uint32_t bit_depth;     // NEW FIELD - breaks compatibility!
    double tempo;
    double position;
};

void get_info(PluginInfo*) {}
void process(AudioContext*) {}
int get_parameter_count() { return 0; }
void get_parameter(int, PluginParameter*) {}
void set_parameter(int, float) {}
void initialize(uint32_t) {}
void shutdown() {}

// Create interface with WRONG hashes (from V2 structs)
PluginInterface create_interface() {
    return PluginInterface{
        // These hashes are from the V2 structs - they WON'T match!
        .plugin_info_hash = get_layout_hash<PluginInfoV2>(),
        .audio_context_hash = get_layout_hash<AudioContextV2>(),
        .plugin_parameter_hash = get_layout_hash<PluginParameter>(),
        .get_info = get_info,
        .process = process,
        .get_parameter_count = get_parameter_count,
        .get_parameter = get_parameter,
        .set_parameter = set_parameter,
        .initialize = initialize,
        .shutdown = shutdown
    };
}

} // namespace incompatible_plugin

// =============================================================================
// Demo
// =============================================================================

void demo_compatible_plugin() {
    std::cout << "\n=== Demo: Loading Compatible Plugin ===\n\n";
    
    PluginHost host;
    auto iface = compatible_plugin::create_interface();
    
    if (host.load_plugin(&iface, "compatible_filter.so")) {
        // Process some audio
        float input[64] = {1.0f, 0.5f, -0.5f, -1.0f};
        float output[64] = {};
        host.process_audio(input, output, 16);
        
        std::cout << "[Host] Processed audio: output[0] = " << output[0] << "\n";
        
        host.unload();
        std::cout << "[Host] Plugin unloaded successfully\n";
    }
}

void demo_incompatible_plugin() {
    std::cout << "\n=== Demo: Loading INCOMPATIBLE Plugin ===\n\n";
    
    std::cout << "[Info] This plugin was compiled with different struct definitions.\n";
    std::cout << "[Info] Without TypeLayout, this would cause silent data corruption.\n\n";
    
    PluginHost host;
    auto iface = incompatible_plugin::create_interface();
    
    if (!host.load_plugin(&iface, "incompatible_filter.so")) {
        std::cout << "\n[Demo] TypeLayout correctly prevented loading an incompatible plugin!\n";
    }
}

void show_layout_info() {
    std::cout << "\n=== Interface Layout Information ===\n\n";
    
    std::cout << "PluginInfo:\n";
    std::cout << "  Size: " << sizeof(PluginInfo) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PluginInfo>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature_cstr<PluginInfo>() << "\n\n";
    
    std::cout << "AudioContext:\n";
    std::cout << "  Size: " << sizeof(AudioContext) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<AudioContext>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature_cstr<AudioContext>() << "\n\n";
    
    std::cout << "PluginParameter:\n";
    std::cout << "  Size: " << sizeof(PluginParameter) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<PluginParameter>() << std::dec << "\n";
    std::cout << "  Signature: " << get_layout_signature_cstr<PluginParameter>() << "\n\n";
    
    // Show incompatible versions for comparison
    std::cout << "--- Incompatible Versions (from bad plugin) ---\n\n";
    
    std::cout << "PluginInfoV2 (incompatible):\n";
    std::cout << "  Size: " << sizeof(incompatible_plugin::PluginInfoV2) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<incompatible_plugin::PluginInfoV2>() << std::dec << "\n\n";
    
    std::cout << "AudioContextV2 (incompatible):\n";
    std::cout << "  Size: " << sizeof(incompatible_plugin::AudioContextV2) << " bytes\n";
    std::cout << "  Hash: 0x" << std::hex << get_layout_hash<incompatible_plugin::AudioContextV2>() << std::dec << "\n";
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Boost.TypeLayout - Plugin/DLL Interface Verification Demo    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    show_layout_info();
    demo_compatible_plugin();
    demo_incompatible_plugin();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Key Takeaway: TypeLayout detects ABI mismatches at load time ║\n";
    std::cout << "║  preventing crashes and silent data corruption from plugins.  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
