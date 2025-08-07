#include "niceshot.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <csetjmp>
#include <png.h>
#include <zlib.h>

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
    double buffer_addr = static_cast<double>(reinterpret_cast<uintptr_t>(test_pixels.data()));
    return niceshot_save_png(buffer_addr, test_width, test_height, "test_output.png");
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
    
    // Cast buffer pointer to pixel data
    uint8_t* pixels = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(buffer_ptr));
    uint32_t img_width = static_cast<uint32_t>(width);
    uint32_t img_height = static_cast<uint32_t>(height);
    
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
    
    // Set PNG header information
    png_set_IHDR(png_ptr, info_ptr, 
                 img_width, img_height,
                 8, // bit depth
                 PNG_COLOR_TYPE_RGBA, // color type (RGBA)
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    
    // Set compression level (0-9, 6 is good balance of speed/size)
    png_set_compression_level(png_ptr, 6);
    
    // Write header
    png_write_info(png_ptr, info_ptr);
    
    // Prepare row pointers
    std::vector<png_bytep> row_pointers(img_height);
    uint32_t stride = img_width * 4; // RGBA = 4 bytes per pixel
    
    for (uint32_t y = 0; y < img_height; ++y) {
        row_pointers[y] = pixels + (y * stride);
    }
    
    // Write image data
    png_write_image(png_ptr, row_pointers.data());
    
    // Finish writing
    png_write_end(png_ptr, nullptr);
    
    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    std::cout << "[NiceShot] PNG saved successfully: " << filepath << " (" << img_width << "x" << img_height << ")" << std::endl;
    
    return 1.0; // Success
}

} // extern "C"