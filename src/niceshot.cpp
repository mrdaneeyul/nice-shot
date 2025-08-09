#include "niceshot.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <csetjmp>
#include <cstdio>
#include <sstream>
#include <png.h>
#include <zlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string>
#include <deque>
#include <chrono>
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

// Performance configuration
static std::atomic<int> g_compression_level{6}; // Default PNG compression level
static std::atomic<size_t> g_thread_count{0}; // 0 = auto-detect based on CPU cores

// Video recording configuration
static std::atomic<int> g_video_preset{1}; // 0=ultrafast, 1=fast, 2=medium, 3=slow, 4=slower

// Async PNG Job System
enum class JobStatus {
    QUEUED = 0,
    PROCESSING = 1,
    COMPLETED = 2,
    FAILED = -1
};

struct PngJob {
    uint32_t job_id;
    std::vector<uint8_t> buffer_data;  // Copied buffer data for thread safety
    uint32_t width;
    uint32_t height;
    std::string filepath;
    JobStatus status;
    std::string error_message;
    
    PngJob(uint32_t id, const uint8_t* pixels, uint32_t w, uint32_t h, const std::string& path)
        : job_id(id), width(w), height(h), filepath(path), status(JobStatus::QUEUED)
    {
        // Copy buffer data for thread safety
        size_t buffer_size = static_cast<size_t>(width) * height * 4; // RGBA = 4 bytes per pixel
        buffer_data.resize(buffer_size);
        std::memcpy(buffer_data.data(), pixels, buffer_size);
    }
};

// Video Recording System
enum class RecordingStatus {
    NOT_RECORDING = 0,
    RECORDING = 1,
    FINALIZING = 2,
    ERROR_STATE = -1
};

struct VideoFrame {
    std::vector<uint8_t> pixel_data;
    uint32_t width;
    uint32_t height;
    std::chrono::high_resolution_clock::time_point timestamp;
    uint64_t frame_number;
    
    VideoFrame(const uint8_t* pixels, uint32_t w, uint32_t h, uint64_t frame_num)
        : width(w), height(h), frame_number(frame_num), timestamp(std::chrono::high_resolution_clock::now())
    {
        size_t buffer_size = static_cast<size_t>(width) * height * 4; // RGBA
        pixel_data.resize(buffer_size);
        std::memcpy(pixel_data.data(), pixels, buffer_size);
    }
    
    size_t get_memory_size() const {
        return pixel_data.size() + sizeof(VideoFrame);
    }
};

struct VideoRecordingSession {
    // Recording parameters
    uint32_t width;
    uint32_t height;
    double fps;
    double bitrate_kbps;
    std::string output_filepath;
    size_t max_buffer_frames;
    
    // Ring buffer for frames
    std::deque<std::unique_ptr<VideoFrame>> frame_buffer;
    std::mutex buffer_mutex;
    std::condition_variable buffer_condition;
    
    // Recording state
    RecordingStatus status;
    uint64_t frames_captured;
    uint64_t frames_encoded;
    uint64_t frames_dropped;
    std::chrono::high_resolution_clock::time_point recording_start_time;
    
    // Memory management
    std::atomic<size_t> current_buffer_memory;
    size_t max_buffer_memory;
    
    // Worker threads
    std::thread encoding_thread;
    std::atomic<bool> stop_encoding;
    
    VideoRecordingSession(uint32_t w, uint32_t h, double f, double bitrate, size_t max_frames, const std::string& filepath)
        : width(w), height(h), fps(f), bitrate_kbps(bitrate), output_filepath(filepath), max_buffer_frames(max_frames),
          status(RecordingStatus::NOT_RECORDING), frames_captured(0), frames_encoded(0), frames_dropped(0),
          current_buffer_memory(0), stop_encoding(false)
    {
        // Calculate maximum memory usage: frame_size * max_frames + overhead
        size_t frame_size = static_cast<size_t>(width) * height * 4 + sizeof(VideoFrame);
        max_buffer_memory = frame_size * max_buffer_frames;
        
        std::cout << "[NiceShot] Video session created: " << width << "x" << height << "@" << fps << "fps" << std::endl;
        std::cout << "[NiceShot] Max buffer frames: " << max_buffer_frames 
                  << " (≈" << (max_buffer_memory / 1024 / 1024) << "MB)" << std::endl;
    }
};

// Global video recording state
static std::unique_ptr<VideoRecordingSession> g_recording_session = nullptr;
static std::mutex g_recording_mutex;

// Global async system state
static std::atomic<uint32_t> g_next_job_id{1};
static std::queue<std::shared_ptr<PngJob>> g_job_queue;
static std::unordered_map<uint32_t, std::shared_ptr<PngJob>> g_active_jobs;
static std::mutex g_job_mutex;
static std::condition_variable g_job_condition;
static std::vector<std::thread> g_worker_threads;
static std::atomic<bool> g_worker_thread_running{false};
static std::atomic<bool> g_shutdown_requested{false};

// Forward declarations for internal functions
static bool encode_png_to_file(const uint8_t* pixels, uint32_t width, uint32_t height, const std::string& filepath, std::string& error_message);
static void worker_thread_main();
static void video_encoding_thread_main(VideoRecordingSession* session);

// Worker thread main function
static void worker_thread_main() {
    std::cout << "[NiceShot] Worker thread started" << std::endl;
    
    while (!g_shutdown_requested.load()) {
        std::shared_ptr<PngJob> job = nullptr;
        
        // Get next job from queue
        {
            std::unique_lock<std::mutex> lock(g_job_mutex);
            
            // Wait for job or shutdown signal
            g_job_condition.wait(lock, [] { 
                return !g_job_queue.empty() || g_shutdown_requested.load(); 
            });
            
            if (g_shutdown_requested.load() && g_job_queue.empty()) {
                break; // Exit thread
            }
            
            if (!g_job_queue.empty()) {
                job = g_job_queue.front();
                g_job_queue.pop();
                job->status = JobStatus::PROCESSING;
            }
        }
        
        // Process job outside the lock
        if (job) {
            std::cout << "[NiceShot] Processing job " << job->job_id << ": " << job->filepath << std::endl;
            
            // Encode PNG using existing logic
            bool success = encode_png_to_file(
                job->buffer_data.data(),
                job->width,
                job->height,
                job->filepath,
                job->error_message
            );
            
            // Update job status
            {
                std::lock_guard<std::mutex> lock(g_job_mutex);
                job->status = success ? JobStatus::COMPLETED : JobStatus::FAILED;
                if (success) {
                    std::cout << "[NiceShot] Job " << job->job_id << " completed successfully" << std::endl;
                } else {
                    std::cout << "[NiceShot] Job " << job->job_id << " failed: " << job->error_message << std::endl;
                }
            }
        }
    }
    
    std::cout << "[NiceShot] Worker thread exiting" << std::endl;
}

// Video encoding thread main function (placeholder without x264 for now)
static void video_encoding_thread_main(VideoRecordingSession* session) {
    if (!session) {
        std::cerr << "[NiceShot] Video encoding thread started with null session" << std::endl;
        return;
    }
    
    std::cout << "[NiceShot] Video encoding thread started for " << session->output_filepath << std::endl;
    
    while (!session->stop_encoding.load()) {
        std::unique_ptr<VideoFrame> frame = nullptr;
        
        // Get next frame from buffer
        {
            std::unique_lock<std::mutex> lock(session->buffer_mutex);
            
            // Wait for frame or stop signal
            session->buffer_condition.wait(lock, [session] {
                return !session->frame_buffer.empty() || session->stop_encoding.load();
            });
            
            if (session->stop_encoding.load() && session->frame_buffer.empty()) {
                break; // Exit thread
            }
            
            if (!session->frame_buffer.empty()) {
                frame = std::move(session->frame_buffer.front());
                session->frame_buffer.pop_front();
                
                // Update memory tracking
                session->current_buffer_memory.fetch_sub(frame->get_memory_size());
            }
        }
        
        // Process frame outside the lock
        if (frame) {
            // TODO: Replace this placeholder with actual H.264 encoding
            // For now, just simulate encoding time and update counters
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Simulate encoding time
            
            session->frames_encoded++;
            
            // Progress logging every 60 frames
            if (session->frames_encoded % 60 == 0) {
                double elapsed = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now() - session->recording_start_time).count();
                double fps_actual = session->frames_encoded / elapsed;
                
                std::cout << "[NiceShot] Encoded " << session->frames_encoded << " frames "
                          << "(avg " << fps_actual << " fps, buffer: " 
                          << session->frame_buffer.size() << " frames)" << std::endl;
            }
        }
    }
    
    std::cout << "[NiceShot] Video encoding thread finished. Encoded " 
              << session->frames_encoded << " frames" << std::endl;
}

// PNG encoding function extracted from niceshot_save_png
static bool encode_png_to_file(const uint8_t* pixels, uint32_t width, uint32_t height, const std::string& filepath, std::string& error_message) {
    // Open file for writing
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, filepath.c_str(), "wb");
#else
    fp = fopen(filepath.c_str(), "wb");
#endif
    
    if (!fp) {
        error_message = "Failed to open file for writing: " + filepath;
        return false;
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        error_message = "Failed to create PNG write structure";
        fclose(fp);
        return false;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        error_message = "Failed to create PNG info structure";
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return false;
    }
    
    // Error handling with setjmp/longjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        error_message = "PNG encoding error occurred";
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return false;
    }
    
    // Set up PNG file writing
    png_init_io(png_ptr, fp);
    
    // Set PNG header information
    png_set_IHDR(png_ptr, info_ptr, 
                 width, height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_set_compression_level(png_ptr, g_compression_level.load());
    png_write_info(png_ptr, info_ptr);
    
    // Write image data row by row
    uint32_t stride = width * 4;
    for (uint32_t y = 0; y < height; ++y) {
        png_bytep row = const_cast<png_bytep>(pixels + (y * stride));
        png_write_row(png_ptr, row);
    }
    
    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    return true;
}

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
        
        // Determine optimal thread count if not set
        size_t thread_count = g_thread_count.load();
        if (thread_count == 0) {
            size_t hw_threads = std::thread::hardware_concurrency();
            if (hw_threads == 0) hw_threads = 1; // Fallback if unable to detect
            if (hw_threads > 8) hw_threads = 8; // Cap at 8 threads
            thread_count = hw_threads;
            g_thread_count = thread_count;
        }
        
        // Reset shutdown flag and start worker threads
        g_shutdown_requested = false;
        g_worker_thread_running = true;
        
        g_worker_threads.clear();
        g_worker_threads.reserve(thread_count);
        
        for (size_t i = 0; i < thread_count; ++i) {
            g_worker_threads.emplace_back(worker_thread_main);
        }
        
        g_initialized = true;
        std::cout << "[NiceShot] Extension initialized successfully with " << thread_count << " worker threads" << std::endl;
        std::cout << "[NiceShot] PNG compression level: " << g_compression_level.load() << std::endl;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Initialization failed: " << e.what() << std::endl;
        g_worker_thread_running = false;
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
        
        // Signal worker threads to shutdown
        g_shutdown_requested = true;
        g_job_condition.notify_all(); // Wake up all worker threads
        
        // Wait for all worker threads to finish
        for (auto& thread : g_worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        g_worker_threads.clear();
        g_worker_thread_running = false;
        
        // Clear any remaining jobs
        {
            std::lock_guard<std::mutex> lock(g_job_mutex);
            while (!g_job_queue.empty()) {
                g_job_queue.pop();
            }
            g_active_jobs.clear();
        }
        
        // Reset job ID counter
        g_next_job_id = 1;
        
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
    
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    // Create a simple 100x100 test image with RGBA pattern
    const uint32_t test_width = 100;
    const uint32_t test_height = 100;
    
    // Create test image data directly without going through string conversion
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
    
    // Save PNG directly without using the string-based interface
    // Open file for writing
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, "test_output.png", "wb");
#else
    fp = fopen("test_output.png", "wb");
#endif
    
    if (!fp) {
        std::cerr << "[NiceShot] Failed to open test file for writing" << std::endl;
        return 0.0;
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "[NiceShot] Failed to create PNG write structure for test" << std::endl;
        fclose(fp);
        return 0.0;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "[NiceShot] Failed to create PNG info structure for test" << std::endl;
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return 0.0;
    }
    
    // Error handling with setjmp/longjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "[NiceShot] PNG encoding error occurred in test" << std::endl;
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0.0;
    }
    
    // Set up PNG file writing
    png_init_io(png_ptr, fp);
    
    // Set PNG header information
    png_set_IHDR(png_ptr, info_ptr, 
                 test_width, test_height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_set_compression_level(png_ptr, g_compression_level.load());
    png_write_info(png_ptr, info_ptr);
    
    // Write image data row by row
    uint32_t stride = test_width * 4;
    for (uint32_t y = 0; y < test_height; ++y) {
        png_bytep row = test_pixels.data() + (y * stride);
        png_write_row(png_ptr, row);
    }
    
    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    std::cout << "[NiceShot] Test PNG saved successfully: test_output.png (" << test_width << "x" << test_height << ")" << std::endl;
    
    return 1.0; // Success
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

// Async PNG functions
double niceshot_save_png_async(const char* buffer_ptr_str, double width, double height, const char* filepath) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    if (!buffer_ptr_str || width <= 0 || height <= 0 || !filepath) {
        std::cerr << "[NiceShot] Invalid parameters for async PNG save" << std::endl;
        return 0.0;
    }
    
    // Parse buffer pointer from string
    uintptr_t buffer_addr = 0;
    if (sscanf(buffer_ptr_str, "%llx", &buffer_addr) != 1 || buffer_addr == 0) {
        std::cerr << "[NiceShot] Invalid buffer pointer string for async save: " << buffer_ptr_str << std::endl;
        return 0.0;
    }
    
    uint8_t* pixels = reinterpret_cast<uint8_t*>(buffer_addr);
    uint32_t img_width = static_cast<uint32_t>(width);
    uint32_t img_height = static_cast<uint32_t>(height);
    
    // Generate unique job ID
    uint32_t job_id = g_next_job_id.fetch_add(1);
    
    try {
        // Create job with copied buffer data
        auto job = std::make_shared<PngJob>(job_id, pixels, img_width, img_height, std::string(filepath));
        
        // Queue job
        {
            std::lock_guard<std::mutex> lock(g_job_mutex);
            g_job_queue.push(job);
            g_active_jobs[job_id] = job;
        }
        
        // Notify worker thread
        g_job_condition.notify_one();
        
        std::cout << "[NiceShot] Queued async PNG job " << job_id << ": " << filepath 
                  << " (" << img_width << "x" << img_height << ")" << std::endl;
        
        return static_cast<double>(job_id);
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Failed to queue async PNG job: " << e.what() << std::endl;
        return 0.0;
    }
}

double niceshot_get_job_status(double job_id) {
    if (!g_initialized) {
        return -2.0; // Not initialized
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return -2.0; // Invalid job ID
    }
    
    std::lock_guard<std::mutex> lock(g_job_mutex);
    auto it = g_active_jobs.find(id);
    if (it == g_active_jobs.end()) {
        return -2.0; // Job not found
    }
    
    return static_cast<double>(static_cast<int>(it->second->status));
}

double niceshot_cleanup_job(double job_id) {
    if (!g_initialized) {
        return 0.0;
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(g_job_mutex);
    auto it = g_active_jobs.find(id);
    if (it == g_active_jobs.end()) {
        return 0.0; // Job not found
    }
    
    // Only cleanup completed or failed jobs
    if (it->second->status == JobStatus::COMPLETED || it->second->status == JobStatus::FAILED) {
        g_active_jobs.erase(it);
        return 1.0; // Success
    }
    
    return 0.0; // Job still processing
}

double niceshot_get_pending_job_count() {
    if (!g_initialized) {
        return -1.0;
    }
    
    std::lock_guard<std::mutex> lock(g_job_mutex);
    return static_cast<double>(g_job_queue.size());
}

double niceshot_worker_thread_status() {
    return g_worker_thread_running.load() ? 1.0 : 0.0;
}

// Performance tuning functions
double niceshot_set_compression_level(double compression_level) {
    int level = static_cast<int>(compression_level);
    if (level < 0 || level > 9) {
        std::cerr << "[NiceShot] Invalid compression level: " << level << " (must be 0-9)" << std::endl;
        return 0.0;
    }
    
    g_compression_level = level;
    std::cout << "[NiceShot] PNG compression level set to: " << level << std::endl;
    return 1.0;
}

double niceshot_get_compression_level() {
    if (!g_initialized) {
        return -1.0;
    }
    return static_cast<double>(g_compression_level.load());
}

double niceshot_set_thread_count(double thread_count) {
    if (g_initialized) {
        std::cerr << "[NiceShot] Cannot change thread count while extension is initialized" << std::endl;
        return 0.0;
    }
    
    size_t count = static_cast<size_t>(thread_count);
    if (count < 1 || count > 8) {
        std::cerr << "[NiceShot] Invalid thread count: " << count << " (must be 1-8)" << std::endl;
        return 0.0;
    }
    
    g_thread_count = count;
    std::cout << "[NiceShot] Worker thread count set to: " << count << std::endl;
    return 1.0;
}

double niceshot_get_thread_count() {
    if (!g_initialized) {
        return -1.0;
    }
    return static_cast<double>(g_thread_count.load());
}

double niceshot_benchmark_png(double width, double height, double iterations) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized for benchmark" << std::endl;
        return -1.0;
    }
    
    uint32_t img_width = static_cast<uint32_t>(width);
    uint32_t img_height = static_cast<uint32_t>(height);
    uint32_t iter_count = static_cast<uint32_t>(iterations);
    
    if (img_width == 0 || img_height == 0 || iter_count == 0) {
        std::cerr << "[NiceShot] Invalid benchmark parameters" << std::endl;
        return -1.0;
    }
    
    std::cout << "[NiceShot] Starting PNG benchmark: " << img_width << "x" << img_height 
              << " x" << iter_count << " iterations" << std::endl;
    std::cout << "[NiceShot] Compression level: " << g_compression_level.load() << std::endl;
    std::cout << "[NiceShot] Worker threads: " << g_thread_count.load() << std::endl;
    
    // Create test image data
    std::vector<uint8_t> test_pixels(img_width * img_height * 4);
    
    // Create a pattern for testing (gradient + noise for realistic compression)
    for (uint32_t y = 0; y < img_height; ++y) {
        for (uint32_t x = 0; x < img_width; ++x) {
            uint32_t index = (y * img_width + x) * 4;
            test_pixels[index + 0] = static_cast<uint8_t>((x * 255) / img_width); // Red gradient
            test_pixels[index + 1] = static_cast<uint8_t>((y * 255) / img_height); // Green gradient
            test_pixels[index + 2] = static_cast<uint8_t>((x + y) % 256); // Blue pattern
            test_pixels[index + 3] = 255; // Alpha opaque
        }
    }
    
    // Run benchmark iterations
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<uint32_t> job_ids;
    for (uint32_t i = 0; i < iter_count; ++i) {
        std::string filepath = "benchmark_" + std::to_string(i) + ".png";
        
        // Create job manually to avoid string conversion overhead
        uint32_t job_id = g_next_job_id.fetch_add(1);
        
        try {
            auto job = std::make_shared<PngJob>(job_id, test_pixels.data(), img_width, img_height, filepath);
            
            {
                std::lock_guard<std::mutex> lock(g_job_mutex);
                g_job_queue.push(job);
                g_active_jobs[job_id] = job;
            }
            
            job_ids.push_back(job_id);
            g_job_condition.notify_one();
        }
        catch (const std::exception& e) {
            std::cerr << "[NiceShot] Benchmark failed to create job " << i << ": " << e.what() << std::endl;
            return -1.0;
        }
    }
    
    // Wait for all jobs to complete
    bool all_complete = false;
    while (!all_complete) {
        all_complete = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        std::lock_guard<std::mutex> lock(g_job_mutex);
        for (uint32_t job_id : job_ids) {
            auto it = g_active_jobs.find(job_id);
            if (it != g_active_jobs.end() && 
                it->second->status != JobStatus::COMPLETED && 
                it->second->status != JobStatus::FAILED) {
                all_complete = false;
                break;
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double avg_time = total_time / iter_count;
    
    // Clean up benchmark jobs
    {
        std::lock_guard<std::mutex> lock(g_job_mutex);
        for (uint32_t job_id : job_ids) {
            g_active_jobs.erase(job_id);
        }
    }
    
    std::cout << "[NiceShot] Benchmark completed in " << total_time << "ms" << std::endl;
    std::cout << "[NiceShot] Average time per PNG: " << avg_time << "ms" << std::endl;
    std::cout << "[NiceShot] Throughput: " << (1000.0 / avg_time) << " PNG/sec" << std::endl;
    
    return avg_time;
}

// Video Recording Functions

double niceshot_start_recording(const char* settings_str, const char* filepath) {
    if (!g_initialized) {
        std::cerr << "[NiceShot] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    if (!settings_str || !filepath) {
        std::cerr << "[NiceShot] Invalid recording parameters (null strings)" << std::endl;
        return 0.0;
    }
    
    // Parse settings string arguments to numeric values: "width,height,fps,bitrate,buffer_frames"
    // Example: "1920,1080,60,5000,120"
    std::string settings(settings_str);
    std::vector<std::string> params;
    std::stringstream ss(settings);
    std::string item;

    while (std::getline(ss, item, ',')) {
        params.push_back(item);
    }

    if (params.size() != 5) {
        std::cerr << "[NiceShot] Invalid settings format. Expected 'width,height,fps,bitrate,buffer_frames'" << std::endl;
    }

    double width = std::atof(params[0].c_str());
    double height = std::atof(params[1].c_str());
    double fps = std::atof(params[2].c_str());
    double bitrate_kbps = std::atof(params[3].c_str());
    double max_buffer_frames = std::atof(params[4].c_str());
    
    if (width <= 0 || height <= 0 || fps <= 0 || bitrate_kbps <= 0 || max_buffer_frames <= 0) {
        std::cerr << "[NiceShot] Invalid recording parameters after parsing: " 
                  << "width=" << width << ", height=" << height << ", fps=" << fps 
                  << ", bitrate=" << bitrate_kbps << ", buffer_frames=" << max_buffer_frames << std::endl;
        return 0.0;
    }
    
    std::cout << "[NiceShot] Parsed recording parameters: " << width << "x" << height 
              << "@" << fps << "fps, " << bitrate_kbps << "kbps, " 
              << max_buffer_frames << " buffer frames" << std::endl;
    
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    // Check if already recording
    if (g_recording_session && g_recording_session->status == RecordingStatus::RECORDING) {
        std::cerr << "[NiceShot] Already recording. Stop current recording first." << std::endl;
        return 0.0;
    }
    
    try {
        // Create new recording session
        uint32_t w = static_cast<uint32_t>(width);
        uint32_t h = static_cast<uint32_t>(height);
        size_t max_frames = static_cast<size_t>(max_buffer_frames);
        
        g_recording_session = std::make_unique<VideoRecordingSession>(w, h, fps, bitrate_kbps, max_frames, std::string(filepath));
        
        // Start encoding thread
        g_recording_session->stop_encoding = false;
        g_recording_session->status = RecordingStatus::RECORDING;
        g_recording_session->recording_start_time = std::chrono::high_resolution_clock::now();
        g_recording_session->encoding_thread = std::thread(video_encoding_thread_main, g_recording_session.get());
        
        std::cout << "[NiceShot] Video recording started: " << filepath << std::endl;
        return 1.0;
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Failed to start recording: " << e.what() << std::endl;
        g_recording_session.reset();
        return 0.0;
    }
}

double niceshot_record_frame(const char* buffer_ptr_str) {
    if (!buffer_ptr_str) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    if (!g_recording_session || g_recording_session->status != RecordingStatus::RECORDING) {
        return 0.0; // Not recording
    }
    
    // Parse buffer pointer from string
    uintptr_t buffer_addr = 0;
    if (sscanf(buffer_ptr_str, "%llx", &buffer_addr) != 1 || buffer_addr == 0) {
        std::cerr << "[NiceShot] Invalid buffer pointer for video frame: " << buffer_ptr_str << std::endl;
        return 0.0;
    }
    
    uint8_t* pixels = reinterpret_cast<uint8_t*>(buffer_addr);
    
    // Check if buffer is full (memory-based limit)
    size_t frame_size = static_cast<size_t>(g_recording_session->width) * g_recording_session->height * 4 + sizeof(VideoFrame);
    if (g_recording_session->current_buffer_memory.load() + frame_size > g_recording_session->max_buffer_memory) {
        // Buffer full - drop this frame
        g_recording_session->frames_dropped++;
        
        if (g_recording_session->frames_dropped % 30 == 1) { // Log every 30 drops
            std::cout << "[NiceShot] Warning: Dropping frames due to buffer full. Dropped " 
                      << g_recording_session->frames_dropped << " frames so far." << std::endl;
        }
        
        return -1.0; // Frame dropped
    }
    
    try {
        // Create frame
        auto frame = std::make_unique<VideoFrame>(pixels, g_recording_session->width, 
                                                 g_recording_session->height, g_recording_session->frames_captured);
        
        // Add to buffer
        {
            std::lock_guard<std::mutex> buffer_lock(g_recording_session->buffer_mutex);
            g_recording_session->current_buffer_memory.fetch_add(frame->get_memory_size());
            g_recording_session->frame_buffer.push_back(std::move(frame));
        }
        
        // Notify encoding thread
        g_recording_session->buffer_condition.notify_one();
        
        g_recording_session->frames_captured++;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Failed to record frame: " << e.what() << std::endl;
        return 0.0;
    }
}

double niceshot_stop_recording() {
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    if (!g_recording_session || g_recording_session->status != RecordingStatus::RECORDING) {
        return 0.0; // Not recording
    }
    
    try {
        std::cout << "[NiceShot] Stopping video recording..." << std::endl;
        
        g_recording_session->status = RecordingStatus::FINALIZING;
        
        // Signal encoding thread to stop
        g_recording_session->stop_encoding = true;
        g_recording_session->buffer_condition.notify_all();
        
        // Wait for encoding thread to finish
        if (g_recording_session->encoding_thread.joinable()) {
            g_recording_session->encoding_thread.join();
        }
        
        // Log final statistics
        double elapsed = std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - g_recording_session->recording_start_time).count();
        double avg_fps = g_recording_session->frames_captured / elapsed;
        
        std::cout << "[NiceShot] Recording finished:" << std::endl;
        std::cout << "[NiceShot]   Duration: " << elapsed << " seconds" << std::endl;
        std::cout << "[NiceShot]   Frames captured: " << g_recording_session->frames_captured << std::endl;
        std::cout << "[NiceShot]   Frames encoded: " << g_recording_session->frames_encoded << std::endl;
        std::cout << "[NiceShot]   Frames dropped: " << g_recording_session->frames_dropped << std::endl;
        std::cout << "[NiceShot]   Average FPS: " << avg_fps << std::endl;
        
        g_recording_session.reset();
        return 1.0;
    }
    catch (const std::exception& e) {
        std::cerr << "[NiceShot] Failed to stop recording: " << e.what() << std::endl;
        g_recording_session.reset();
        return 0.0;
    }
}

double niceshot_get_recording_buffer_usage() {
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    if (!g_recording_session || g_recording_session->status != RecordingStatus::RECORDING) {
        return -1.0;
    }
    
    double usage_percent = (static_cast<double>(g_recording_session->current_buffer_memory.load()) / 
                           g_recording_session->max_buffer_memory) * 100.0;
    return usage_percent;
}

double niceshot_get_recording_frame_count() {
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    if (!g_recording_session) {
        return -1.0;
    }
    
    return static_cast<double>(g_recording_session->frames_captured);
}

double niceshot_get_recording_status() {
    std::lock_guard<std::mutex> lock(g_recording_mutex);
    
    if (!g_recording_session) {
        return static_cast<double>(RecordingStatus::NOT_RECORDING);
    }
    
    return static_cast<double>(g_recording_session->status);
}

double niceshot_set_video_preset(double preset) {
    int preset_int = static_cast<int>(preset);
    if (preset_int < 0 || preset_int > 4) {
        std::cerr << "[NiceShot] Invalid video preset: " << preset_int << " (must be 0-4)" << std::endl;
        return 0.0;
    }
    
    g_video_preset = preset_int;
    
    const char* preset_names[] = {"ultrafast", "fast", "medium", "slow", "slower"};
    std::cout << "[NiceShot] Video preset set to: " << preset_names[preset_int] << std::endl;
    return 1.0;
}

} // extern "C"