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
    
    // Performance tuning functions
    
    // Set PNG compression level (0=fastest, 9=smallest, 6=default)
    // Parameters: compression_level (0-9)
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_set_compression_level(double compression_level);
    
    // Get current compression level
    // Returns: current compression level (0-9), -1.0 if not initialized
    NICESHOT_API double niceshot_get_compression_level();
    
    // Set number of worker threads (1-8, defaults to CPU cores)
    // Parameters: thread_count (1-8)
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_set_thread_count(double thread_count);
    
    // Get current thread count
    // Returns: current thread count, -1.0 if not initialized
    NICESHOT_API double niceshot_get_thread_count();
    
    // Benchmark function - test PNG encoding performance
    // Parameters: width, height, iteration_count
    // Returns: average encode time in milliseconds, -1.0 on error
    NICESHOT_API double niceshot_benchmark_png(double width, double height, double iterations);
    
    // Video Recording Functions
    
    // Start video recording with specified settings
    // Parameters: width, height, fps, bitrate_kbps, max_buffer_frames, filepath
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_start_recording(double width, double height, double fps, double bitrate_kbps, double max_buffer_frames, const char* filepath);
    
    // Record a single frame (call this every frame during recording)
    // Parameters: buffer_ptr_str (GameMaker buffer address as string)
    // Returns: 1.0 on success, 0.0 on failure, -1.0 if buffer full (frame dropped)
    NICESHOT_API double niceshot_record_frame(const char* buffer_ptr_str);
    
    // Stop video recording and finalize file
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_stop_recording();
    
    // Get recording statistics
    // Returns: buffer_usage_percent (0-100), -1.0 if not recording
    NICESHOT_API double niceshot_get_recording_buffer_usage();
    
    // Get number of frames recorded
    // Returns: frame count, -1.0 if not recording
    NICESHOT_API double niceshot_get_recording_frame_count();
    
    // Get recording status
    // Returns: 0=not recording, 1=recording, 2=finalizing, -1=error
    NICESHOT_API double niceshot_get_recording_status();
    
    // Set video quality preset (call before start_recording)
    // Parameters: preset (0=ultrafast, 1=fast, 2=medium, 3=slow, 4=slower)
    // Returns: 1.0 on success, 0.0 on failure
    NICESHOT_API double niceshot_set_video_preset(double preset);
}