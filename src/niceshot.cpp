#include "niceshot.h"
#include <iostream>

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        std::cout << "[NiceShot] DLL attached" << std::endl;
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        std::cout << "[NiceShot] DLL detached" << std::endl;
        break;
    }
    return TRUE;
}

// Global state
static bool g_initialized = false;

// GameMaker interface implementation
extern "C" {

double niceshot_init() {
    if (g_initialized) {
        std::cout << "[NiceShot] Already initialized" << std::endl;
        return 1.0; // Already initialized
    }
    
    try {
        std::cout << "[NiceShot] Initializing extension..." << std::endl;
        
        // TODO: Initialize libpng, thread pool, etc.
        
        g_initialized = true;
        std::cout << "[NiceShot] Extension initialized successfully" << std::endl;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Initialization failed: " << e.what() << std::endl;
        return 0.0; // Failure
    }
}

double niceshot_shutdown() {
    if (!g_initialized) {
        std::cout << "[NiceShot] Not initialized, nothing to shutdown" << std::endl;
        return 1.0; // Nothing to shutdown
    }
    
    try {
        std::cout << "[NiceShot] Shutting down extension..." << std::endl;
        
        // TODO: Cleanup libpng, thread pool, etc.
        
        g_initialized = false;
        std::cout << "[NiceShot] Extension shutdown successfully" << std::endl;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Shutdown failed: " << e.what() << std::endl;
        return 0.0; // Failure
    }
}

double niceshot_test(double input) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return -1.0; // Error - not initialized
    }
    
    std::cout << "[NiceShot] Test function called with input: " << input << std::endl;
    return input + 1.0; // Simple test: return input + 1
}

const char* niceshot_get_version() {
    static const char* version = "NiceShot v0.1.0 - Development Build";
    return version;
}

double niceshot_save_png(double buffer_ptr, double width, double height, const char* filepath) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    if (buffer_ptr <= 0 || width <= 0 || height <= 0 || !filepath) {
        std::cerr << "[NiceShot] Invalid parameters for PNG save" << std::endl;
        return 0.0;
    }
    
    std::cout << "[NiceShot] PNG save requested: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    
    // TODO: Implement actual PNG encoding with libpng
    // For now, just simulate success
    std::cout << "[NiceShot] PNG save simulated - would save " << (int)(width * height * 4) << " bytes" << std::endl;
    
    return 1.0; // Success (simulated)
}

} // extern "C"