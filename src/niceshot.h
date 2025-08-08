#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Export macro for GameMaker functions
#ifdef NICESHOT_EXPORTS
#define NICESHOT_API __declspec(dllexport)
#else
#define NICESHOT_API __declspec(dllimport)
#endif

// C interface for GameMaker (no C++ name mangling)
extern "C" {
    // Initialize the extension (called once on game start)
    NICESHOT_API double niceshot_init();
    
    // Cleanup the extension (called on game end)  
    NICESHOT_API double niceshot_shutdown();
    
    // Test function - returns the input value + 1 (for testing connection)
    NICESHOT_API double niceshot_test(double input);
    
    // Get version string (returns pointer to static string)
    NICESHOT_API const char* niceshot_get_version();
    
    // Test function - creates and saves a simple test PNG
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_test_png();
    
    // Basic PNG save function (synchronous)
    // Parameters: buffer_ptr_str (GameMaker buffer address as string), width, height, filepath
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_save_png(const char* buffer_ptr_str, double width, double height, const char* filepath);
    
    // Async PNG Functions
    
    // Save PNG asynchronously (returns immediately with job ID)
    // Parameters: buffer_ptr_str (GameMaker buffer address as string), width, height, filepath
    // Returns: job_id (>0) on success, 0.0 on failure
    NICESHOT_API double niceshot_save_png_async(const char* buffer_ptr_str, double width, double height, const char* filepath);
    
    // Get status of async PNG job
    // Parameters: job_id
    // Returns: 0=queued, 1=processing, 2=completed, -1=failed, -2=not_found/invalid
    NICESHOT_API double niceshot_get_job_status(double job_id);
    
    // Cleanup completed/failed job (frees memory)
    // Parameters: job_id
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_cleanup_job(double job_id);
    
    // Get number of jobs waiting in queue
    // Returns: number of pending jobs, -1.0 if not initialized
    NICESHOT_API double niceshot_get_pending_job_count();
    
    // Check if worker thread is running
    // Returns: 1.0 if running, 0.0 if stopped
    NICESHOT_API double niceshot_worker_thread_status();
}