# Async PNG Extension Implementation Plan

## Overview

This document outlines the complete implementation of a custom GameMaker Studio 2 extension for asynchronous PNG image saving. The extension will eliminate frame drops during screenshot capture and enable high-performance frame-by-frame recording for video generation.

## Core Requirements

### Phase 1 (Immediate - Windows Only)
1. **Asynchronous PNG Encoding**: Multi-threaded PNG compression and file writing
2. **GameMaker Integration**: Seamless GML function interface  
3. **Windows DLL**: Single platform for immediate use
4. **Memory Management**: Safe buffer handling and cleanup
5. **High Performance**: Suitable for 60 FPS frame-by-frame recording
6. **Error Handling**: Robust failure detection and reporting

### Phase 2 (Marketplace Expansion)
1. **Cross-Platform Support**: Linux and macOS compatibility
2. **Documentation**: Complete API documentation and examples
3. **Marketplace Package**: Professional packaging with demos
4. **Advanced Features**: Progress callbacks, batch operations

## Technical Architecture

### Extension Structure (Phase 1 - Windows Only)

```
extensions/AsyncPNG/
├── AsyncPNG.yy                          # GameMaker extension definition
├── AsyncPNG.dll                         # Windows 64-bit DLL
├── libs/                                # Dependencies
│   ├── libpng.dll                       # PNG encoding library
│   └── zlib.dll                         # Compression library
├── src/                                 # Source code (for reference/building)
│   ├── async_png.cpp                    # Core implementation
│   ├── async_png.h                      # Header definitions
│   ├── png_encoder.cpp                  # PNG encoding logic
│   ├── thread_pool.cpp                  # Thread management
│   └── gm_interface.cpp                 # GameMaker interface
├── AsyncPNG.vcxproj                     # Visual Studio project
├── AsyncPNG.sln                         # Visual Studio solution
└── README.md                            # Basic usage instructions
```

### Future Marketplace Structure (Phase 2)

```
extensions/AsyncPNG/
├── AsyncPNG.yy                          # GameMaker extension definition
├── windows/AsyncPNG.dll                 # Windows DLL
├── linux/libAsyncPNG.so                 # Linux shared library  
├── macos/libAsyncPNG.dylib              # macOS dynamic library
├── docs/
│   ├── API_Reference.md                 # Complete function documentation
│   ├── Getting_Started.md               # Tutorial and examples
│   ├── Performance_Guide.md             # Optimization tips
│   └── Troubleshooting.md               # Common issues
├── examples/
│   ├── screenshot_demo.gml              # Basic screenshot example
│   ├── recording_demo.gml               # Frame-by-frame recording
│   └── AsyncPNG_Demo.yyp                # Complete demo project
└── changelog.md                         # Version history
```

### Core Components

#### 1. Thread Pool Management

**Purpose**: Manage worker threads for PNG encoding without overwhelming the system

```cpp
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop_flag;
    static const size_t max_threads = 4;
    
public:
    ThreadPool();
    ~ThreadPool();
    
    template<class F>
    auto enqueue(F&& f) -> std::future<typename std::result_of<F()>::type>;
    
    void shutdown();
    size_t pending_tasks();
};

// Global thread pool instance
static ThreadPool* g_thread_pool = nullptr;
```

#### 2. PNG Encoding Engine

**Libraries**: 
- **libpng** - Industry standard PNG encoding
- **zlib** - Compression backend for PNG

```cpp
class PNGEncoder {
public:
    struct ImageData {
        uint8_t* pixels;      // RGBA pixel data
        uint32_t width;       // Image width
        uint32_t height;      // Image height
        uint32_t stride;      // Bytes per row
    };
    
    struct EncodeResult {
        bool success;
        std::string error_message;
        size_t file_size;
        double encode_time_ms;
    };
    
    static EncodeResult encode_to_file(const ImageData& image, const std::string& filepath);
    static EncodeResult encode_to_buffer(const ImageData& image, std::vector<uint8_t>& output);
    
private:
    static void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length);
    static void png_error_callback(png_structp png_ptr, png_const_charp error_msg);
    static void png_warning_callback(png_structp png_ptr, png_const_charp warning_msg);
};
```

#### 3. Save Job Management

**Purpose**: Track async save operations and provide status queries

```cpp
struct SaveJob {
    uint32_t job_id;
    std::string filepath;
    std::future<PNGEncoder::EncodeResult> future;
    std::chrono::high_resolution_clock::time_point start_time;
    
    enum class Status {
        PENDING,    // Queued but not started
        RUNNING,    // Currently encoding
        COMPLETED,  // Successfully saved
        FAILED,     // Error occurred
        CANCELLED   // Job was cancelled
    };
    
    Status get_status() const;
    double get_progress_percent() const;  // Estimate based on typical encode times
    EncodeResult get_result();            // Blocks if not complete
};

class SaveJobManager {
private:
    std::unordered_map<uint32_t, std::unique_ptr<SaveJob>> active_jobs;
    std::mutex jobs_mutex;
    uint32_t next_job_id = 1;
    
public:
    uint32_t create_job(const std::string& filepath);
    void start_job(uint32_t job_id, const PNGEncoder::ImageData& image);
    SaveJob::Status get_job_status(uint32_t job_id);
    bool is_job_complete(uint32_t job_id);
    PNGEncoder::EncodeResult get_job_result(uint32_t job_id);
    void cleanup_job(uint32_t job_id);
    void cleanup_completed_jobs();
    std::vector<uint32_t> get_all_job_ids();
};

// Global job manager
static SaveJobManager* g_job_manager = nullptr;
```

#### 4. GameMaker Interface Layer

**Purpose**: Expose C functions that GameMaker can call

```cpp
// C interface for GameMaker (no C++ name mangling)
extern "C" {
    // Initialize the extension (called once on game start)
    __declspec(dllexport) double async_png_init();
    
    // Cleanup the extension (called on game end)
    __declspec(dllexport) double async_png_shutdown();
    
    // Start async PNG save from surface buffer
    // Parameters: buffer_pointer, width, height, filepath
    // Returns: job_id (0 = failure)
    __declspec(dllexport) double async_png_save_buffer(double buffer_ptr, double width, double height, char* filepath);
    
    // Check if a save job is complete
    // Parameters: job_id
    // Returns: 1.0 = complete, 0.0 = still running, -1.0 = error/not found
    __declspec(dllexport) double async_png_is_complete(double job_id);
    
    // Get job status details
    // Parameters: job_id
    // Returns: 0=pending, 1=running, 2=completed, 3=failed, 4=cancelled, -1=not found
    __declspec(dllexport) double async_png_get_status(double job_id);
    
    // Get job progress percentage (0.0 to 100.0)
    // Parameters: job_id
    // Returns: progress percentage, -1.0 = not found
    __declspec(dllexport) double async_png_get_progress(double job_id);
    
    // Cleanup completed job (frees memory)
    // Parameters: job_id
    // Returns: 1.0 = success, 0.0 = failure
    __declspec(dllexport) double async_png_cleanup_job(double job_id);
    
    // Get number of pending jobs (for monitoring)
    // Returns: number of jobs still in progress
    __declspec(dllexport) double async_png_get_pending_count();
    
    // Emergency: cancel all jobs and cleanup
    // Returns: number of jobs cancelled
    __declspec(dllexport) double async_png_cancel_all();
}
```

### GameMaker Integration (GML)

#### Extension Definition (AsyncPNG.yy)

```json
{
  "$GMExtension": "",
  "%Name": "AsyncPNG",
  "description": "Asynchronous PNG image saving extension with multi-threaded encoding",
  "extensionVersion": "1.0.0",
  "files": [
    {
      "$GMExtensionFile": "",
      "%Name": "AsyncPNG.dll",
      "functions": [
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_init",
          "argCount": 0,
          "args": [],
          "externalName": "async_png_init",
          "help": "async_png_init() - Initialize the async PNG extension",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_shutdown", 
          "argCount": 0,
          "args": [],
          "externalName": "async_png_shutdown",
          "help": "async_png_shutdown() - Shutdown the async PNG extension",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_save_buffer",
          "argCount": 4,
          "args": [2, 2, 2, 1],
          "externalName": "async_png_save_buffer", 
          "help": "async_png_save_buffer(buffer_ptr, width, height, filepath) - Start async PNG save",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_is_complete",
          "argCount": 1,
          "args": [2],
          "externalName": "async_png_is_complete",
          "help": "async_png_is_complete(job_id) - Check if save is complete",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_get_status",
          "argCount": 1,
          "args": [2], 
          "externalName": "async_png_get_status",
          "help": "async_png_get_status(job_id) - Get detailed job status",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_get_progress",
          "argCount": 1,
          "args": [2],
          "externalName": "async_png_get_progress", 
          "help": "async_png_get_progress(job_id) - Get progress percentage",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_cleanup_job",
          "argCount": 1,
          "args": [2],
          "externalName": "async_png_cleanup_job",
          "help": "async_png_cleanup_job(job_id) - Cleanup completed job",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_get_pending_count",
          "argCount": 0,
          "args": [],
          "externalName": "async_png_get_pending_count",
          "help": "async_png_get_pending_count() - Get number of pending jobs",
          "returnType": 2
        },
        {
          "$GMExtensionFunction": "",
          "%Name": "async_png_cancel_all",
          "argCount": 0,
          "args": [],
          "externalName": "async_png_cancel_all", 
          "help": "async_png_cancel_all() - Cancel all pending jobs",
          "returnType": 2
        }
      ],
      "init": "async_png_init",
      "final": "async_png_shutdown"
    }
  ]
}
```

#### GML Wrapper Functions

```gml
/// @description Initialize async PNG extension (called automatically)
function __async_png_initialize() {
    if (!variable_global_exists("__async_png_initialized")) {
        global.__async_png_initialized = false;
        global.__async_png_jobs = {};
        global.__async_png_cleanup_timer = 0;
    }
    
    if (!global.__async_png_initialized) {
        var _result = async_png_init();
        if (_result > 0) {
            global.__async_png_initialized = true;
            show_debug_message("AsyncPNG extension initialized successfully");
        } else {
            show_debug_message("AsyncPNG extension failed to initialize");
        }
    }
}

/// @description Save surface asynchronously as PNG
/// @param {id.Surface} surface - The surface to save
/// @param {string} filepath - Path to save the PNG file
/// @returns {real} job_id - Job ID for tracking (0 = failure)
function surface_save_async_png(_surface, _filepath) {
    if (!global.__async_png_initialized) {
        show_debug_message("AsyncPNG not initialized");
        return 0;
    }
    
    if (!surface_exists(_surface)) {
        show_debug_message("AsyncPNG: Invalid surface");
        return 0;
    }
    
    // Create buffer from surface
    var _width = surface_get_width(_surface);
    var _height = surface_get_height(_surface);
    var _buffer = buffer_create(_width * _height * 4, buffer_fixed, 1);
    
    if (!buffer_exists(_buffer)) {
        show_debug_message("AsyncPNG: Failed to create buffer");
        return 0;
    }
    
    // Copy surface to buffer
    buffer_get_surface(_buffer, _surface, 0);
    
    // Start async save
    var _job_id = async_png_save_buffer(buffer_get_address(_buffer), _width, _height, _filepath);
    
    if (_job_id > 0) {
        // Store job info for cleanup
        global.__async_png_jobs[$ _job_id] = {
            buffer: _buffer,
            filepath: _filepath,
            width: _width,
            height: _height,
            start_time: current_time
        };
        
        show_debug_message($"AsyncPNG: Started job {_job_id} for {_filepath}");
    } else {
        // Cleanup buffer if job failed to start
        buffer_delete(_buffer);
        show_debug_message($"AsyncPNG: Failed to start job for {_filepath}");
    }
    
    return _job_id;
}

/// @description Check if async PNG save is complete
/// @param {real} job_id - The job ID returned by surface_save_async_png
/// @returns {bool} true if complete, false if still running
function async_png_save_is_complete(_job_id) {
    if (!global.__async_png_initialized || _job_id <= 0) {
        return false;
    }
    
    var _status = async_png_is_complete(_job_id);
    return _status > 0;
}

/// @description Get detailed status of async PNG save job
/// @param {real} job_id - The job ID to check
/// @returns {struct} Status information: {status, progress, complete, error}
function async_png_save_get_info(_job_id) {
    var _info = {
        status: "unknown",
        progress: 0,
        complete: false,
        error: false,
        found: false
    };
    
    if (!global.__async_png_initialized || _job_id <= 0) {
        return _info;
    }
    
    var _status_code = async_png_get_status(_job_id);
    var _progress = async_png_get_progress(_job_id);
    
    if (_status_code < 0) {
        _info.status = "not_found";
        return _info;
    }
    
    _info.found = true;
    _info.progress = max(0, _progress);
    
    switch (_status_code) {
        case 0: 
            _info.status = "pending";
            break;
        case 1: 
            _info.status = "running";
            break;
        case 2: 
            _info.status = "completed";
            _info.complete = true;
            break;
        case 3: 
            _info.status = "failed";
            _info.error = true;
            _info.complete = true;
            break;
        case 4: 
            _info.status = "cancelled";
            _info.complete = true;
            break;
    }
    
    return _info;
}

/// @description Cleanup completed async PNG job (call after completion)
/// @param {real} job_id - The job ID to cleanup
/// @returns {bool} true if successfully cleaned up
function async_png_save_cleanup(_job_id) {
    if (!global.__async_png_initialized || _job_id <= 0) {
        return false;
    }
    
    // Check if we have job info
    if (variable_struct_exists(global.__async_png_jobs, string(_job_id))) {
        var _job_info = global.__async_png_jobs[$ string(_job_id)];
        
        // Free the buffer
        if (buffer_exists(_job_info.buffer)) {
            buffer_delete(_job_info.buffer);
        }
        
        // Remove from tracking
        variable_struct_remove(global.__async_png_jobs, string(_job_id));
    }
    
    // Cleanup job in extension
    var _result = async_png_cleanup_job(_job_id);
    
    if (_result > 0) {
        show_debug_message($"AsyncPNG: Cleaned up job {_job_id}");
        return true;
    } else {
        show_debug_message($"AsyncPNG: Failed to cleanup job {_job_id}");
        return false;
    }
}

/// @description Get number of pending async PNG jobs
/// @returns {real} Number of jobs still in progress
function async_png_get_pending_jobs() {
    if (!global.__async_png_initialized) {
        return 0;
    }
    
    return async_png_get_pending_count();
}

/// @description Auto-cleanup completed jobs (call in step event of manager object)
function async_png_auto_cleanup() {
    if (!global.__async_png_initialized) {
        return;
    }
    
    // Only check every 30 frames to avoid overhead
    global.__async_png_cleanup_timer++;
    if (global.__async_png_cleanup_timer < 30) {
        return;
    }
    global.__async_png_cleanup_timer = 0;
    
    var _job_ids = variable_struct_get_names(global.__async_png_jobs);
    var _cleaned_count = 0;
    
    for (var i = 0; i < array_length(_job_ids); i++) {
        var _job_id = real(_job_ids[i]);
        var _info = async_png_save_get_info(_job_id);
        
        if (_info.complete) {
            if (async_png_save_cleanup(_job_id)) {
                _cleaned_count++;
            }
        }
    }
    
    if (_cleaned_count > 0) {
        show_debug_message($"AsyncPNG: Auto-cleaned {_cleaned_count} completed jobs");
    }
}
```

#### Manager Object Integration

```gml
// obj_async_png_manager Create Event
__async_png_initialize();

// obj_async_png_manager Step Event  
async_png_auto_cleanup();

// obj_async_png_manager CleanUp Event
if (global.__async_png_initialized) {
    var _cancelled = async_png_cancel_all();
    if (_cancelled > 0) {
        show_debug_message($"AsyncPNG: Cancelled {_cancelled} jobs on cleanup");
    }
}
```

## C++ Implementation Details

### Core Implementation (async_png.cpp)

```cpp
#include "async_png.h"
#include "png_encoder.h"
#include "thread_pool.h"
#include <chrono>
#include <iostream>
#include <fstream>

// Global instances
ThreadPool* g_thread_pool = nullptr;
SaveJobManager* g_job_manager = nullptr;
bool g_initialized = false;

// GameMaker interface implementation
extern "C" {

double async_png_init() {
    if (g_initialized) {
        return 1.0; // Already initialized
    }
    
    try {
        g_thread_pool = new ThreadPool();
        g_job_manager = new SaveJobManager();
        g_initialized = true;
        
        std::cout << "[AsyncPNG] Extension initialized successfully" << std::endl;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[AsyncPNG] Initialization failed: " << e.what() << std::endl;
        return 0.0; // Failure
    }
}

double async_png_shutdown() {
    if (!g_initialized) {
        return 1.0; // Nothing to shutdown
    }
    
    try {
        if (g_thread_pool) {
            g_thread_pool->shutdown();
            delete g_thread_pool;
            g_thread_pool = nullptr;
        }
        
        if (g_job_manager) {
            delete g_job_manager;
            g_job_manager = nullptr;
        }
        
        g_initialized = false;
        
        std::cout << "[AsyncPNG] Extension shutdown successfully" << std::endl;
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[AsyncPNG] Shutdown failed: " << e.what() << std::endl;
        return 0.0; // Failure
    }
}

double async_png_save_buffer(double buffer_ptr, double width, double height, char* filepath) {
    if (!g_initialized || !g_job_manager || !g_thread_pool) {
        std::cerr << "[AsyncPNG] Extension not initialized" << std::endl;
        return 0.0;
    }
    
    if (buffer_ptr <= 0 || width <= 0 || height <= 0 || !filepath) {
        std::cerr << "[AsyncPNG] Invalid parameters" << std::endl;
        return 0.0;
    }
    
    try {
        // Create job
        uint32_t job_id = g_job_manager->create_job(std::string(filepath));
        
        // Prepare image data
        PNGEncoder::ImageData image;
        image.pixels = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(buffer_ptr));
        image.width = static_cast<uint32_t>(width);
        image.height = static_cast<uint32_t>(height);
        image.stride = image.width * 4; // RGBA
        
        // Start async encoding
        g_job_manager->start_job(job_id, image);
        
        std::cout << "[AsyncPNG] Started job " << job_id << " for " << filepath << std::endl;
        return static_cast<double>(job_id);
    }
    catch (const std::exception& e) {
        std::cerr << "[AsyncPNG] Failed to start job: " << e.what() << std::endl;
        return 0.0;
    }
}

double async_png_is_complete(double job_id) {
    if (!g_initialized || !g_job_manager) {
        return -1.0; // Not initialized
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return -1.0; // Invalid job ID
    }
    
    return g_job_manager->is_job_complete(id) ? 1.0 : 0.0;
}

double async_png_get_status(double job_id) {
    if (!g_initialized || !g_job_manager) {
        return -1.0; // Not initialized
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return -1.0; // Invalid job ID
    }
    
    SaveJob::Status status = g_job_manager->get_job_status(id);
    return static_cast<double>(static_cast<int>(status));
}

double async_png_get_progress(double job_id) {
    if (!g_initialized || !g_job_manager) {
        return -1.0; // Not initialized
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return -1.0; // Invalid job ID
    }
    
    try {
        // For now, return simple progress based on job status
        SaveJob::Status status = g_job_manager->get_job_status(id);
        switch (status) {
            case SaveJob::Status::PENDING: return 0.0;
            case SaveJob::Status::RUNNING: return 50.0; // Estimate
            case SaveJob::Status::COMPLETED: return 100.0;
            case SaveJob::Status::FAILED: return 100.0;
            case SaveJob::Status::CANCELLED: return 100.0;
            default: return -1.0;
        }
    }
    catch (const std::exception& e) {
        return -1.0;
    }
}

double async_png_cleanup_job(double job_id) {
    if (!g_initialized || !g_job_manager) {
        return 0.0; // Not initialized
    }
    
    uint32_t id = static_cast<uint32_t>(job_id);
    if (id == 0) {
        return 0.0; // Invalid job ID
    }
    
    try {
        g_job_manager->cleanup_job(id);
        return 1.0; // Success
    }
    catch (const std::exception& e) {
        std::cerr << "[AsyncPNG] Cleanup failed for job " << job_id << ": " << e.what() << std::endl;
        return 0.0; // Failure
    }
}

double async_png_get_pending_count() {
    if (!g_initialized || !g_thread_pool) {
        return 0.0;
    }
    
    return static_cast<double>(g_thread_pool->pending_tasks());
}

double async_png_cancel_all() {
    if (!g_initialized || !g_job_manager) {
        return 0.0;
    }
    
    try {
        auto job_ids = g_job_manager->get_all_job_ids();
        size_t cancelled = 0;
        
        for (uint32_t id : job_ids) {
            try {
                g_job_manager->cleanup_job(id);
                cancelled++;
            }
            catch (...) {
                // Continue with other jobs
            }
        }
        
        return static_cast<double>(cancelled);
    }
    catch (const std::exception& e) {
        std::cerr << "[AsyncPNG] Cancel all failed: " << e.what() << std::endl;
        return 0.0;
    }
}

} // extern "C"
```

### PNG Encoder Implementation (png_encoder.cpp)

```cpp
#include "png_encoder.h"
#include <png.h>
#include <fstream>
#include <chrono>
#include <iostream>

PNGEncoder::EncodeResult PNGEncoder::encode_to_file(const ImageData& image, const std::string& filepath) {
    EncodeResult result;
    result.success = false;
    result.file_size = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Open file for writing
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, filepath.c_str(), "wb");
#else
    fp = fopen(filepath.c_str(), "wb");
#endif
    
    if (!fp) {
        result.error_message = "Failed to open file for writing: " + filepath;
        return result;
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_callback, png_warning_callback);
    if (!png_ptr) {
        result.error_message = "Failed to create PNG write structure";
        fclose(fp);
        return result;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        result.error_message = "Failed to create PNG info structure";
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return result;
    }
    
    // Error handling with setjmp/longjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        result.error_message = "PNG encoding error occurred";
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return result;
    }
    
    // Set up PNG file writing
    png_init_io(png_ptr, fp);
    
    // Set PNG header information
    png_set_IHDR(png_ptr, info_ptr, 
                 image.width, image.height,
                 8, // bit depth
                 PNG_COLOR_TYPE_RGBA, // color type
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    
    // Set compression level (0-9, 6 is good balance of speed/size)
    png_set_compression_level(png_ptr, 6);
    
    // Write header
    png_write_info(png_ptr, info_ptr);
    
    // Prepare row pointers
    std::vector<png_bytep> row_pointers(image.height);
    for (uint32_t y = 0; y < image.height; ++y) {
        row_pointers[y] = image.pixels + (y * image.stride);
    }
    
    // Write image data
    png_write_image(png_ptr, row_pointers.data());
    
    // Finish writing
    png_write_end(png_ptr, nullptr);
    
    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    
    // Calculate timing and file size
    auto end_time = std::chrono::high_resolution_clock::now();
    result.encode_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Get file size
    std::ifstream file(filepath, std::ifstream::ate | std::ifstream::binary);
    if (file.is_open()) {
        result.file_size = file.tellg();
        file.close();
    }
    
    result.success = true;
    return result;
}

void PNGEncoder::png_error_callback(png_structp png_ptr, png_const_charp error_msg) {
    std::cerr << "[AsyncPNG] PNG Error: " << error_msg << std::endl;
    longjmp(png_jmpbuf(png_ptr), 1);
}

void PNGEncoder::png_warning_callback(png_structp png_ptr, png_const_charp warning_msg) {
    std::cout << "[AsyncPNG] PNG Warning: " << warning_msg << std::endl;
}
```

### Thread Pool Implementation (thread_pool.cpp)

```cpp
#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool() : stop_flag(false) {
    size_t num_threads = std::min(max_threads, std::thread::hardware_concurrency());
    if (num_threads == 0) num_threads = 2; // Fallback
    
    workers.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { return stop_flag || !tasks.empty(); });
                    
                    if (stop_flag && tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                
                try {
                    task();
                }
                catch (const std::exception& e) {
                    std::cerr << "[AsyncPNG] Thread pool task error: " << e.what() << std::endl;
                }
                catch (...) {
                    std::cerr << "[AsyncPNG] Thread pool unknown task error" << std::endl;
                }
            }
        });
    }
    
    std::cout << "[AsyncPNG] Thread pool created with " << num_threads << " threads" << std::endl;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop_flag = true;
    }
    
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers.clear();
    
    // Clear remaining tasks
    std::queue<std::function<void()>> empty;
    tasks.swap(empty);
    
    std::cout << "[AsyncPNG] Thread pool shutdown complete" << std::endl;
}

size_t ThreadPool::pending_tasks() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
}

template<class F>
auto ThreadPool::enqueue(F&& f) -> std::future<typename std::result_of<F()>::type> {
    using return_type = typename std::result_of<F()>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<F>(f)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        if (stop_flag) {
            throw std::runtime_error("ThreadPool is shutting down");
        }
        
        tasks.emplace([task]() { (*task)(); });
    }
    
    condition.notify_one();
    return result;
}

// Explicit instantiations for common use cases
template auto ThreadPool::enqueue<std::function<void()>>(std::function<void()>&&) 
    -> std::future<void>;
template auto ThreadPool::enqueue<std::function<PNGEncoder::EncodeResult()>>(std::function<PNGEncoder::EncodeResult()>&&) 
    -> std::future<PNGEncoder::EncodeResult>;
```

## Usage Examples

### Screenshot System Integration

```gml
// Modified screenshot system using async PNG extension
if (screenshotPending) {
    var _directory = screenshotDirectory;
    
    if (!directory_exists(_directory)) {
        directory_create(_directory);
    }
    
    var _fullPath = _directory + pendingScreenshotName;
    
    // Create composite surface with all effects (same as before)
    var _screenshotSurface = surface_create(window_get_width(), window_get_height());
    
    surface_set_target(_screenshotSurface);
    {
        // Recreate final composite rendering
        var _zoom = min(global.zoomLevel, global.maxZoomLevel);
        var _width = IDEAL_WIDTH * _zoom;
        var _height = IDEAL_HEIGHT * _zoom;
        var _x = (window_get_width() - _width)/2;
        var _y = (window_get_height() - _height)/2;
        
        draw_clear(c_black);
        
        crt_shader_set();
        {
            draw_surface_stretched(application_surface, _x, _y, _width, _height);
            if (surface_exists(global.lightingSurface))
                draw_surface(global.lightingSurface, _x, _y);
            if (surface_exists(global.darknessSurface) && get_region_lighting() == lightingType.dark)
                draw_surface_stretched(global.darknessSurface, _x, _y, _width, _height);
            draw_surface_stretched(global.applicationSurfaceGUI, _x, _y, _width, _height);
            draw_surface_stretched(global.applicationSurfaceTransition, _x, _y, _width, _height);
        }
        shader_reset();
    }
    surface_reset_target();
    
    // Start async PNG save - NO FRAME DROP!
    var _job_id = surface_save_async_png(_screenshotSurface, _fullPath);
    
    if (_job_id > 0) {
        show_debug_message("Screenshot queued for async save (job " + string(_job_id) + "): " + _fullPath);
    } else {
        show_debug_message("Screenshot failed to queue: " + _fullPath);
        surface_free(_screenshotSurface);
    }
    
    screenshotPending = false;
    pendingScreenshotName = "";
    screenshotDirectory = "";
}
```

### Frame-by-Frame Recording System

```gml
// Recording system using async PNG extension
function start_frame_recording(_base_name, _duration_seconds, _fps) {
    if (!global.__async_png_initialized) {
        show_debug_message("AsyncPNG not available for recording");
        return false;
    }
    
    global.recording = {
        active: true,
        base_name: _base_name,
        directory: "recordings/" + _base_name + "/",
        frame_count: 0,
        total_frames: _duration_seconds * _fps,
        fps: _fps,
        frame_jobs: [],
        start_time: current_time
    };
    
    if (!directory_exists(global.recording.directory)) {
        directory_create(global.recording.directory);
    }
    
    show_debug_message($"Started recording: {global.recording.total_frames} frames at {_fps} FPS");
    return true;
}

function record_frame() {
    if (!global.recording.active) return;
    
    // Create composite surface (same as screenshot)
    var _surface = surface_create(window_get_width(), window_get_height());
    
    surface_set_target(_surface);
    {
        // Render complete composite with effects
        var _zoom = min(global.zoomLevel, global.maxZoomLevel);
        var _width = IDEAL_WIDTH * _zoom;
        var _height = IDEAL_HEIGHT * _zoom;
        var _x = (window_get_width() - _width)/2;
        var _y = (window_get_height() - _height)/2;
        
        draw_clear(c_black);
        
        crt_shader_set();
        {
            draw_surface_stretched(application_surface, _x, _y, _width, _height);
            if (surface_exists(global.lightingSurface))
                draw_surface(global.lightingSurface, _x, _y);
            if (surface_exists(global.darknessSurface) && get_region_lighting() == lightingType.dark)
                draw_surface_stretched(global.darknessSurface, _x, _y, _width, _height);
            draw_surface_stretched(global.applicationSurfaceGUI, _x, _y, _width, _height);
            draw_surface_stretched(global.applicationSurfaceTransition, _x, _y, _width, _height);
        }
        shader_reset();
    }
    surface_reset_target();
    
    // Generate frame filename with zero-padding
    var _frame_str = string_replace_all(string_format(global.recording.frame_count, 6, 0), " ", "0");
    var _filename = global.recording.base_name + "_frame_" + _frame_str + ".png";
    var _filepath = global.recording.directory + _filename;
    
    // Start async save - maintains 60 FPS!
    var _job_id = surface_save_async_png(_surface, _filepath);
    
    if (_job_id > 0) {
        array_push(global.recording.frame_jobs, _job_id);
    } else {
        show_debug_message($"Failed to queue frame {global.recording.frame_count}");
        surface_free(_surface);
    }
    
    global.recording.frame_count++;
    
    // Check if recording complete
    if (global.recording.frame_count >= global.recording.total_frames) {
        finish_recording();
    }
}

function finish_recording() {
    if (!global.recording.active) return;
    
    global.recording.active = false;
    
    show_debug_message($"Recording finished: {global.recording.frame_count} frames queued");
    show_debug_message($"Pending async jobs: {async_png_get_pending_jobs()}");
    
    // Monitor completion
    call_later(60, time_source_units_frames, function() {
        monitor_recording_completion();
    });
}

function monitor_recording_completion() {
    var _pending = async_png_get_pending_jobs();
    
    if (_pending > 0) {
        show_debug_message($"Recording: {_pending} frames still encoding...");
        call_later(120, time_source_units_frames, function() {
            monitor_recording_completion();
        });
    } else {
        show_debug_message("Recording complete! All frames saved.");
        
        // Generate processing script
        var _script = {
            input_pattern: global.recording.directory + global.recording.base_name + "_frame_%06d.png",
            output_video: global.recording.directory + global.recording.base_name + ".mp4",
            fps: global.recording.fps,
            total_frames: global.recording.frame_count,
            ffmpeg_command: $"ffmpeg -r {global.recording.fps} -i \"{global.recording.directory + global.recording.base_name}_frame_%06d.png\" -c:v libx264 -pix_fmt yuv420p \"{global.recording.directory + global.recording.base_name}.mp4\""
        };
        
        var _script_file = file_text_open_write(global.recording.directory + "process_video.json");
        file_text_write_string(_script_file, json_stringify(_script, true));
        file_text_close(_script_file);
        
        show_debug_message("Processing script saved: process_video.json");
    }
}
```

## Build Configuration (Phase 1 - Windows Only)

### Quick Start Development Setup

1. **Install Visual Studio 2019/2022** with C++ desktop development
2. **Download libpng binaries**: [libpng-1.6.x Windows binaries](http://gnuwin32.sourceforge.net/packages/libpng.htm)
3. **Download zlib binaries**: [zlib Windows binaries](http://gnuwin32.sourceforge.net/packages/zlib.htm)
4. **Create Visual Studio project** with the configuration below
5. **Build and copy DLL** to GameMaker extension folder

### Windows (Visual Studio)

**AsyncPNG.vcxproj**:
```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>

  <PropertyGroup Label="Globals">
    <ProjectGuid>{12345678-1234-1234-1234-123456789012}</ProjectGuid>
    <TargetFrameworkVersion>v4.0</TargetFrameworkVersion>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>AsyncPNG</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />

  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>

  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />

  <PropertyGroup>
    <IncludePath>$(SolutionDir)libs\libpng\include;$(SolutionDir)libs\zlib\include;$(IncludePath)</IncludePath>
    <LibraryPath>$(SolutionDir)libs\libpng\lib\x64;$(SolutionDir)libs\zlib\lib\x64;$(LibraryPath)</LibraryPath>
  </PropertyGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <Optimization>Disabled</Optimization>
      <PreprocessorDefinitions>WIN32;_DEBUG;_WINDOWS;_USRDLL;ASYNCPNG_EXPORTS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp17</LanguageStandard>
    </ClCompile>
    <Link>
      <SubSystem>Windows</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalDependencies>libpngd.lib;zlibd.lib;kernel32.lib;user32.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <Optimization>MaxSpeed</Optimization>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <PreprocessorDefinitions>WIN32;NDEBUG;_WINDOWS;_USRDLL;ASYNCPNG_EXPORTS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp17</LanguageStandard>
    </ClCompile>
    <Link>
      <SubSystem>Windows</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
      <AdditionalDependencies>libpng.lib;zlib.lib;kernel32.lib;user32.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <ItemGroup>
    <ClInclude Include="src\async_png.h" />
    <ClInclude Include="src\png_encoder.h" />
    <ClInclude Include="src\thread_pool.h" />
  </ItemGroup>

  <ItemGroup>
    <ClCompile Include="src\async_png.cpp" />
    <ClCompile Include="src\png_encoder.cpp" />
    <ClCompile Include="src\thread_pool.cpp" />
    <ClCompile Include="src\gm_interface.cpp" />
  </ItemGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

## Cross-Platform Expansion (Phase 2 - Marketplace)

### Linux Makefile (Future)

```makefile
# AsyncPNG Linux Makefile (for marketplace expansion)
CXX = g++
CXXFLAGS = -std=c++17 -fPIC -Wall -Wextra -O3
LIBS = -lpng -lz -lpthread
TARGET = libAsyncPNG.so

# Build configuration for marketplace distribution
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CXX) -shared -o $@ $^ $(LIBS)
```

### macOS Build (Future)

**Xcode Configuration for Marketplace**:
- **Universal Binary**: x86_64 + arm64 support
- **C++ Standard**: C++17
- **Dependencies**: libpng, zlib (via Homebrew or static linking)

## Performance Characteristics

### Benchmarks (Estimated)

**Single PNG Save (1920x1080 RGBA)**:
- **Synchronous**: ~15-25ms (visible frame drop)
- **Async**: ~0.5-1ms main thread impact + 15-25ms background

**60 FPS Recording (10 seconds)**:
- **Total frames**: 600
- **Memory usage**: ~50MB buffer pool (rotating)
- **Disk I/O**: Distributed over time, no frame drops
- **CPU usage**: 1 core for game, 2-3 cores for encoding

### Memory Management

- **Buffer lifecycle**: GameMaker→Extension→Encoder→Cleanup
- **Maximum concurrent jobs**: Limited by thread pool size
- **Memory pressure handling**: Job queuing when thread pool saturated
- **Auto-cleanup**: Completed jobs automatically freed

## Testing Strategy

### Unit Tests
- PNG encoding correctness
- Thread pool behavior
- Memory leak detection
- Error handling paths

### Integration Tests
- GameMaker interface validation
- Cross-platform compatibility
- Performance under load
- Stress testing (1000+ concurrent saves)

### Performance Tests
- Frame rate impact measurement
- Memory usage profiling
- Disk I/O characteristics
- Threading efficiency

## Future Enhancements

### Phase 2 Features
1. **WebP Support**: Additional format options
2. **Compression Options**: Quality/speed trade-offs
3. **Progress Callbacks**: Real-time encoding progress
4. **Batch Operations**: Multiple surfaces in single job
5. **Memory Pooling**: Reduce allocation overhead

### Phase 3 Features
1. **Video Encoding**: Direct MP4 creation
2. **Audio Integration**: Video with synchronized audio
3. **Streaming**: Real-time video streaming support
4. **GPU Acceleration**: Hardware-accelerated encoding

## Conclusion

This async PNG extension provides a robust foundation for high-performance image saving in GameMaker projects. The multi-threaded architecture eliminates frame drops while maintaining complete visual fidelity, enabling smooth 60 FPS recording and responsive screenshot capture.

The modular design allows for easy extension and cross-platform deployment, making it suitable for both development tools and production features. The comprehensive error handling and memory management ensure stable operation under heavy load.

## Implementation Roadmap

### Phase 1 - Personal Use (Windows Only)
**Priority**: Get it working for ProtoDungeon3 immediately
1. **Core extension** (Windows DLL)
2. **GameMaker integration** (GML wrapper functions) 
3. **Basic testing** (integration with screenshot system)
4. **Performance validation** (frame-by-frame recording)

**Timeline**: 1-2 weeks for basic functionality

### Phase 2 - Marketplace Release
**Priority**: Polish for public release and revenue
1. **Cross-platform builds** (Linux SO, macOS dylib)
2. **Complete documentation** (API reference, tutorials, examples)
3. **Demo project** (showcase all features)
4. **Advanced features** (progress reporting, batch operations)
5. **Professional packaging** (marketplace submission)

**Timeline**: 1-2 months for marketplace-ready version

### Revenue Potential
- **GameMaker Marketplace**: $10-25 pricing
- **Niche market**: Performance-critical screenshot/recording
- **Target users**: Content creators, automated testing, game developers
- **Differentiation**: Only high-performance async PNG solution

## Implementation Progress (Phase 1)

### ✅ Completed Steps

1. **Visual Studio Project Setup** ✅
   - Created `nice-shot` repository at `C:/Users/Daniel/source/repos/nice-shot`
   - Visual Studio 2022 solution with C++17, x64 DLL configuration
   - Project builds successfully with basic C interface

2. **Basic GameMaker Extension Interface** ✅
   - Created DLL with 4 test functions:
     - `niceshot_init()` - Initialization
     - `niceshot_shutdown()` - Cleanup
     - `niceshot_test(input)` - Connection test (returns input + 1)
     - `niceshot_get_version()` - Version string
   - GameMaker extension configured in ProtoDungeon3
   - **Successfully tested**: Extension loads and functions work correctly

3. **Repository Setup** ✅
   - GitHub repository `nice-shot` created
   - Proper .gitignore configured (excludes .vs/, bin/, obj/, etc.)
   - Clean separation from ProtoDungeon3 main project

4. **PNG Save Function Stub** ✅
   - Added `niceshot_save_png(buffer_ptr, width, height, filepath)` function
   - Currently simulates PNG save (returns success without actual encoding)
   - Ready for libpng integration

### 🎯 Current Status
**Extension works perfectly** - GameMaker can call C++ DLL functions successfully!

**Test Output**:
```
=== NiceShot Extension Test ===
[NiceShot] Test function called with input: 42
Test function: 42 -> 43
Version: NiceShot v0.1.0 - Development Build
```

## Immediate Next Steps (Phase 1)

### 🔄 In Progress - Add PNG Encoding

1. **Add libpng Dependencies**
   - Download libpng and zlib Windows binaries
   - Configure Visual Studio project with library paths
   - Update linker settings to include libpng.lib and zlib.lib

2. **Implement Real PNG Encoder**
   - Replace `niceshot_save_png()` stub with actual libpng encoding
   - Handle GameMaker buffer data (RGBA format)
   - Add proper error handling and file I/O

3. **Test with Real Screenshot Data**
   - Create test buffer in GameMaker with surface data
   - Verify PNG files are created correctly
   - Test with ProtoDungeon3 screenshot system

### 📋 Remaining Phase 1 Tasks

4. **Add Threading System**
   - Implement thread pool for background PNG encoding
   - Add async job management with status checking
   - Create async versions of PNG save functions

5. **Integration with Screenshot System**
   - Replace synchronous `screen_save()` in `obj_render_manager`
   - Test frame-by-frame recording capability
   - Verify complete visual effects are preserved

6. **Performance Testing**
   - Benchmark sync vs async performance
   - Test 60 FPS recording capability
   - Memory usage optimization

### 🚀 Next Phase 2 Goals (Marketplace)
- Cross-platform builds (Linux, macOS)
- Professional documentation and demo project  
- Advanced features (progress callbacks, batch operations)
- Marketplace packaging and submission

## Technical Notes

**Current Architecture**:
- **Repository**: `C:/Users/Daniel/source/repos/nice-shot`
- **DLL Output**: `bin/Release/NiceShot.dll` → copied to ProtoDungeon3 extensions
- **GameMaker Integration**: Extension with 5 functions (4 working + 1 PNG stub)
- **Build System**: Visual Studio 2022, C++17, Windows x64

**Key Success**: Extension communication between GameMaker and C++ DLL is fully functional!

This focused approach gets you async PNG saving quickly while keeping the door open for a profitable marketplace extension later!