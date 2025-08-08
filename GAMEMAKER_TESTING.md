# NiceShot PNG Testing Guide for GameMaker

## Current Extension Functions Available

```gml
// Test function - creates a 100x100 gradient PNG
niceshot_test_png()  // Returns 1.0 on success, 0.0 on failure

// Real PNG save function (UPDATED - uses string for buffer pointer)
niceshot_save_png(buffer_ptr_str, width, height, filepath)
// - buffer_ptr_str: GameMaker buffer address as STRING (use string(buffer_get_address()))
// - width, height: Image dimensions
// - filepath: Full path where to save PNG (e.g., "screenshot.png")
// Returns 1.0 on success, 0.0 on failure

// Existing functions still work:
niceshot_init()      // Initialize (should return 1.0)
niceshot_test(42)    // Test connection (should return 43.0) 
niceshot_get_version() // Get version string
```

## Quick Test Code for GameMaker

### Test 1: Simple PNG Creation Test
```gml
// Test 1: Simple PNG creation test
show_debug_message("=== Testing NiceShot PNG Creation ===");

// Initialize extension
var init_result = niceshot_init();
show_debug_message("Init result: " + string(init_result));

// Test PNG creation (creates test_output.png in game directory)
var png_result = niceshot_test_png();
if (png_result > 0) {
    show_debug_message("SUCCESS: test_output.png created!");
} else {
    show_debug_message("FAILED: PNG test failed");
}
```

### Test 2: Save Current Application Surface as PNG
```gml
// Test 2: Save current application surface as PNG
if (surface_exists(application_surface)) {
    // Get surface dimensions
    var surf_w = surface_get_width(application_surface);
    var surf_h = surface_get_height(application_surface);
    
    // Create buffer from surface - use buffer_grow for better stability
    var buffer = buffer_create(surf_w * surf_h * 4, buffer_fixed, 1);
    buffer_get_surface(buffer, application_surface, 0);
    
    // IMPORTANT: Get the address and call the extension immediately
    // Don't do anything else between getting the address and calling the DLL
    var buffer_addr = string(buffer_get_address(buffer));
    var save_result = niceshot_save_png(buffer_addr, surf_w, surf_h, working_directory + "gamemaker_screenshot.png");
    
    // Only cleanup after the PNG save is complete
    buffer_delete(buffer);
    
    if (save_result > 0) {
        show_debug_message("SUCCESS: gamemaker_screenshot.png saved to " + working_directory);
    } else {
        show_debug_message("FAILED: Screenshot save failed");
    }
}
```

## Integration with Screenshot System

Replace your current `screen_save()` call in `obj_render_manager` with:

```gml
// In your screenshot system, replace:
// screen_save(filepath);

// With this:
if (surface_exists(application_surface)) {
    var surf_w = surface_get_width(application_surface);
    var surf_h = surface_get_height(application_surface);
    var buffer = buffer_create(surf_w * surf_h * 4, buffer_fixed, 1);
    buffer_get_surface(buffer, application_surface, 0);
    
    // Get address and call immediately - critical for buffer stability
    var buffer_addr = string(buffer_get_address(buffer));
    var result = niceshot_save_png(buffer_addr, surf_w, surf_h, filepath);
    
    buffer_delete(buffer);
    
    if (result > 0) {
        show_debug_message("NiceShot PNG saved: " + filepath);
    } else {
        show_debug_message("NiceShot PNG failed: " + filepath);
    }
}
```

## DLL Files Required

The updated DLL with PNG functionality is at:
`C:\users\daniel\source\repos\nice-shot\bin\Release\NiceShot.dll`

Make sure to copy these files to your GameMaker extension directory:
- `NiceShot.dll` (main extension)
- `libpng16.dll` (PNG encoding library)
- `zlib1.dll` (compression library)

## Testing Checklist

1. **Basic PNG Test**: Run `niceshot_test_png()` - should create `test_output.png`
2. **Surface Screenshot**: Save application_surface as PNG
3. **File Verification**: Check that PNG files are created and can be opened in image viewer
4. **Error Handling**: Test with invalid parameters to verify error handling
5. **Integration Test**: Replace existing screenshot system calls

## Expected Results

- `test_output.png`: 100x100 image with red-to-right, green-to-bottom gradient
- `gamemaker_screenshot.png`: Current game screen capture
- Console messages showing success/failure for each operation
- PNG files viewable in any image viewer
- No frame drops during PNG creation (synchronous for now)

## Troubleshooting

If you get errors:
- Ensure all 3 DLL files are in the extension directory
- Check that `niceshot_init()` returns 1.0 before calling PNG functions
- Verify file paths are writable (try saving to game directory first)
- Check GameMaker output console for detailed error messages from the extension

## Async PNG Functions (NEW!)

### Available Async Functions
```gml
// Save PNG in background thread (returns immediately)
niceshot_save_png_async(buffer_ptr_str, width, height, filepath) // Returns job_id

// Check job status
niceshot_get_job_status(job_id) // 0=queued, 1=processing, 2=complete, -1=failed

// Cleanup finished job
niceshot_cleanup_job(job_id) // Returns 1.0 on success

// Monitor system
niceshot_get_pending_job_count() // Number of jobs in queue
niceshot_worker_thread_status() // 1.0 if worker thread running
```

### Test 4: Async PNG Saving (Frame-Drop-Free)
```gml
// Test 4: Async PNG saving for frame-drop-free recording
show_debug_message("=== Testing Async PNG Saving ===");

if (surface_exists(application_surface)) {
    var surf_w = surface_get_width(application_surface);
    var surf_h = surface_get_height(application_surface);
    var buffer = buffer_create(surf_w * surf_h * 4, buffer_fixed, 1);
    buffer_get_surface(buffer, application_surface, 0);
    
    // Queue multiple async saves
    var job_ids = [];
    for (var i = 0; i < 5; i++) {
        var filename = working_directory + "async_test_" + string(i) + ".png";
        var job_id = niceshot_save_png_async(
            string(buffer_get_address(buffer)), 
            surf_w, surf_h, filename
        );
        array_push(job_ids, job_id);
        show_debug_message("Queued async job " + string(job_id) + ": " + filename);
    }
    
    buffer_delete(buffer);
    
    // Monitor jobs
    show_debug_message("Worker thread status: " + string(niceshot_worker_thread_status()));
    show_debug_message("Pending jobs: " + string(niceshot_get_pending_job_count()));
    
    // Store job IDs globally to check status later
    global.test_job_ids = job_ids;
    
    show_debug_message("SUCCESS: " + string(array_length(job_ids)) + " async jobs queued!");
}
```

### Job Status Monitoring
```gml
// Check job status periodically (put this in a Step event or alarm)
if (variable_global_exists("test_job_ids")) {
    var completed_jobs = 0;
    var failed_jobs = 0;
    
    for (var i = 0; i < array_length(global.test_job_ids); i++) {
        var job_id = global.test_job_ids[i];
        var status = niceshot_get_job_status(job_id);
        
        if (status == 2) { // Completed
            show_debug_message("Job " + string(job_id) + " completed successfully");
            niceshot_cleanup_job(job_id); // Free memory
            completed_jobs++;
        } else if (status == -1) { // Failed
            show_debug_message("Job " + string(job_id) + " failed");
            niceshot_cleanup_job(job_id); // Free memory
            failed_jobs++;
        }
    }
    
    if (completed_jobs + failed_jobs == array_length(global.test_job_ids)) {
        show_debug_message("All async jobs finished! Completed: " + string(completed_jobs) + ", Failed: " + string(failed_jobs));
        delete global.test_job_ids; // Cleanup
    }
}
```

## Performance Benefits

### Frame-Drop-Free Recording
```gml
// High-performance screenshot system
// Call this every frame or multiple times per frame without frame drops!

if (surface_exists(application_surface)) {
    var surf_w = surface_get_width(application_surface);
    var surf_h = surface_get_height(application_surface);
    var buffer = buffer_create(surf_w * surf_h * 4, buffer_fixed, 1);
    buffer_get_surface(buffer, application_surface, 0);
    
    // Queue save asynchronously - returns immediately!
    var timestamp = string(date_get_year(date_current_datetime())) + 
                   string_replace_all(string(date_get_hour_of_year(date_current_datetime())), " ", "0");
    var filename = working_directory + "frame_" + timestamp + ".png";
    var job_id = niceshot_save_png_async(string(buffer_get_address(buffer)), surf_w, surf_h, filename);
    
    buffer_delete(buffer);
    
    // No waiting! Game continues at full speed
    show_debug_message("Frame captured with job ID: " + string(job_id));
}
```

## Next Steps After Testing

Async PNG system is now implemented! Test the async functionality:
1. ✅ Basic PNG saving (synchronous)
2. ✅ Async PNG saving with job management  
3. ✅ Worker thread background processing
4. 🚧 High-performance frame-by-frame video recording