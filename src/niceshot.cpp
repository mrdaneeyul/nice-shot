#include "niceshot.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <csetjmp>
#include <cstdio>
#include <sstream>
#include <png.h>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

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

__declspec(dllexport) double niceshot_test_libpng() {
    // Test libpng version
    const char* version = png_get_libpng_ver(NULL);
    if (version) {
        printf("[NiceShot] libpng version: %s\n", version);
        return 1.0; // Success
    }
    return 0.0; // Failure
}

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

double niceshot_test_png() {
    std::cout << "[NiceShot] Creating test PNG..." << std::endl;
    
    // Create a simple 100x100 test image with RGBA pattern
    const uint32_t test_width = 100;
    const uint32_t test_height = 100;
    
    // Use std::vector to ensure proper memory allocation
    std::vector<uint8_t> test_pixels(test_width * test_height * 4);
    
    // Create a simple gradient pattern
    for (uint32_t y = 0; y < test_height; ++y) {
        for (uint32_t x = 0; x < test_width; ++x) {
            uint32_t index = (y * test_width + x) * 4;
            test_pixels[index + 0] = static_cast<uint8_t>((x * 255) / test_width); // Red gradient
            test_pixels[index + 1] = static_cast<uint8_t>((y * 255) / test_height); // Green gradient
            test_pixels[index + 2] = 128; // Blue constant
            test_pixels[index + 3] = 255; // Alpha opaque
        }
    }
    
    // Use our PNG save function to test it
    uintptr_t buffer_addr = reinterpret_cast<uintptr_t>(test_pixels.data());
    std::ostringstream oss;
    oss << buffer_addr;
    double result = niceshot_save_png(oss.str().c_str(), test_width, test_height, "test_output.png");
    
    // std::vector automatically cleans up when it goes out of scope
    
    return result;
}

double niceshot_save_png(const char* buffer_ptr_str, double width, double height, const char* filepath) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    if (!buffer_ptr_str || width <= 0 || height <= 0 || !filepath) {
        std::cerr << "[NiceShot] Invalid parameters for PNG save" << std::endl;
        return 0.0;
    }
    
    std::cout << "[NiceShot] PNG save requested: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    std::cout << "[NiceShot] Buffer pointer string: " << buffer_ptr_str << std::endl;
    
    // Parse buffer pointer from string (GameMaker sends it as hex with leading zeros)
    uintptr_t buffer_addr = 0;
    if (sscanf(buffer_ptr_str, "%llx", &buffer_addr) != 1 || buffer_addr == 0) {
        std::cerr << "[NiceShot] Invalid buffer pointer string: " << buffer_ptr_str << std::endl;
        return 0.0;
    }
    
    std::cout << "[NiceShot] Parsed buffer address: 0x" << std::hex << buffer_addr << std::dec << std::endl;
    
    // Validate buffer address is reasonable (not null, not obviously corrupt)
    if (buffer_addr < 0x1000 || buffer_addr > 0x7FFFFFFFFFFF) {
        std::cerr << "[NiceShot] Buffer address appears invalid: 0x" << std::hex << buffer_addr << std::dec << std::endl;
        return 0.0;
    }
    
    // Cast buffer pointer to pixel data
    uint8_t* pixels = reinterpret_cast<uint8_t*>(buffer_addr);
    uint32_t img_width = static_cast<uint32_t>(width);
    uint32_t img_height = static_cast<uint32_t>(height);
    
    // Validate image dimensions are reasonable
    if (img_width > 16384 || img_height > 16384) {
        std::cerr << "[NiceShot] Image dimensions too large: " << img_width << "x" << img_height << std::endl;
        return 0.0;
    }
    
    // Debug: Check if address is properly aligned
    if (buffer_addr % 4 != 0) {
        std::cout << "[NiceShot] WARNING: Buffer address not 4-byte aligned: 0x" << std::hex << buffer_addr << std::dec << std::endl;
    }
    
    // Debug: Try to use Windows VirtualQuery to check if memory is valid
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(pixels, &mbi, sizeof(mbi)) != 0) {
        std::cout << "[NiceShot] Memory info - BaseAddress: 0x" << std::hex << mbi.BaseAddress
                  << ", RegionSize: " << std::dec << mbi.RegionSize
                  << ", State: " << mbi.State 
                  << ", Protect: 0x" << std::hex << mbi.Protect << std::dec << std::endl;
        
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
            std::cerr << "[NiceShot] Memory region is not readable or committed" << std::endl;
            return 0.0;
        }
    } else {
        std::cerr << "[NiceShot] VirtualQuery failed for buffer address" << std::endl;
        return 0.0;
    }
#endif
    
    // Try to safely read first few pixels to validate buffer
    std::cout << "[NiceShot] Attempting to read first pixel at address 0x" << std::hex << buffer_addr << std::dec << std::endl;
    try {
        volatile uint8_t test_read = pixels[0];
        volatile uint8_t test_read2 = pixels[3]; // RGBA, so 4th byte
        std::cout << "[NiceShot] Buffer validation passed, first pixel: " 
                  << (int)pixels[0] << "," << (int)pixels[1] << "," << (int)pixels[2] << "," << (int)pixels[3] << std::endl;
    } catch (...) {
        std::cerr << "[NiceShot] Buffer access test failed - invalid memory" << std::endl;
        return 0.0;
    }
    
    // Open file for writing
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, filepath, "wb");
#else
    fp = fopen(filepath, "wb");
#endif
    
    if (!fp) {
        std::cerr << "[NiceShot] Failed to open file for writing: " << filepath << std::endl;
        return 0.0;
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "[NiceShot] Failed to create PNG write structure" << std::endl;
        fclose(fp);
        return 0.0;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "[NiceShot] Failed to create PNG info structure" << std::endl;
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return 0.0;
    }
    
    // Error handling with setjmp/longjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "[NiceShot] PNG encoding error occurred" << std::endl;
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0.0;
    }
    
    // Set up PNG file writing
    png_init_io(png_ptr, fp);
    
    // Set PNG header information - explicitly disable interlacing
    png_set_IHDR(png_ptr, info_ptr, 
                 img_width, img_height,
                 8, // bit depth
                 PNG_COLOR_TYPE_RGBA, // color type (RGBA)
                 PNG_INTERLACE_NONE, // CRITICAL: No interlacing
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    
    // Set compression level (0-9, 6 is good balance of speed/size)
    png_set_compression_level(png_ptr, 6);
    
    // Write header
    png_write_info(png_ptr, info_ptr);
    
    // Prepare row pointers - write one row at a time to avoid interlacing issues
    uint32_t stride = img_width * 4; // RGBA = 4 bytes per pixel
    
    // Write image data row by row with additional safety checks
    std::cout << "[NiceShot] Writing PNG rows..." << std::endl;
    for (uint32_t y = 0; y < img_height; ++y) {
        png_bytep row = pixels + (y * stride);
        
        // Additional safety: validate row pointer before each write
        if (row < pixels || row >= pixels + (img_width * img_height * 4)) {
            std::cerr << "[NiceShot] Row pointer out of bounds at row " << y << std::endl;
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
            return 0.0;
        }
        
        png_write_row(png_ptr, row);
        
        // Progress indicator for debugging
        if (y % 100 == 0) {
            std::cout << "[NiceShot] Written row " << y << "/" << img_height << std::endl;
        }
    }
    std::cout << "[NiceShot] All rows written successfully" << std::endl;
    
    // Finish writing
    png_write_end(png_ptr, nullptr);
    
    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    std::cout << "[NiceShot] PNG saved successfully: " << filepath << " (" << img_width << "x" << img_height << ")" << std::endl;
    
    return 1.0; // Success
}

} // extern "C"