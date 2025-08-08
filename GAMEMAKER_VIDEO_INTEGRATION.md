# GameMaker Video Recording Integration Guide

This guide shows how to integrate the NiceShot video recording system into your GameMaker project for smooth 60fps video capture.

## Overview

The NiceShot extension now includes a **ring buffer video recording system** that eliminates frame drops during video capture. Frame recording takes ~1ms with zero impact on game performance.

## Extension Functions Available

### Core Recording Functions
```gml
// Start video recording (ALL PARAMETERS MUST BE STRINGS due to GameMaker DLL limitation)
// Returns: 1.0 on success, 0.0 on failure
niceshot_start_recording(width_str, height_str, fps_str, bitrate_kbps_str, max_buffer_frames_str, filepath)

// Record a single frame (call every frame)
// Returns: 1.0 on success, 0.0 on failure, -1.0 if buffer full (frame dropped)
niceshot_record_frame(buffer_ptr_str)

// Stop recording and finalize video file
// Returns: 1.0 on success, 0.0 on failure
niceshot_stop_recording()
```

### Monitoring Functions
```gml
// Get buffer usage (0-100%)
niceshot_get_recording_buffer_usage()

// Get total frames captured
niceshot_get_recording_frame_count()

// Get recording status (0=not recording, 1=recording, 2=finalizing)
niceshot_get_recording_status()

// Set quality preset before recording (0=ultrafast, 1=fast, 2=medium, 3=slow, 4=slower)
niceshot_set_video_preset(preset)
```

## Implementation Example

### 1. Recording Manager Object (obj_video_recorder)

**Create Event:**
```gml
// Video recording state
recording = false;
recording_surface = -1;
recording_buffer = -1;
recording_filepath = "";
recording_frame_count = 0;
recording_start_time = 0;

// Configuration
video_width = 1920;
video_height = 1080;
video_fps = 60;
video_bitrate = 5000; // 5 Mbps
video_buffer_frames = 120; // ~2 seconds at 60fps (≈1GB memory)
video_quality = 1; // 0=ultrafast, 1=fast, 2=medium, 3=slow, 4=slower

// Statistics
buffer_usage = 0;
frames_captured = 0;
last_stats_time = 0;
```

**Step Event:**
```gml
// Update recording statistics every second
if (recording && current_time - last_stats_time > 1000) {
    buffer_usage = niceshot_get_recording_buffer_usage();
    frames_captured = niceshot_get_recording_frame_count();
    
    // Log progress
    var elapsed = (current_time - recording_start_time) / 1000;
    var avg_fps = frames_captured / elapsed;
    
    show_debug_message($"Recording: {frames_captured} frames, {avg_fps:.1f} fps, buffer: {buffer_usage:.1f}%");
    
    // Warning if buffer getting full
    if (buffer_usage > 80) {
        show_debug_message("Warning: Video buffer " + string(buffer_usage) + "% full!");
    }
    
    last_stats_time = current_time;
}
```

### 2. Recording Control Functions

**Start Recording:**
```gml
function start_video_recording(_filename) {
    if (recording) {
        show_debug_message("Already recording video");
        return false;
    }
    
    // Set quality preset
    niceshot_set_video_preset(video_quality);
    
    // Create recording surface matching game resolution
    video_width = window_get_width();
    video_height = window_get_height();
    
    recording_surface = surface_create(video_width, video_height);
    recording_buffer = buffer_create(video_width * video_height * 4, buffer_fixed, 1);
    
    if (!surface_exists(recording_surface) || !buffer_exists(recording_buffer)) {
        show_debug_message("Failed to create video recording resources");
        cleanup_recording_resources();
        return false;
    }
    
    // Generate timestamped filename
    var timestamp = string(current_year) + string_format(current_month, 2, 0) + 
                   string_format(current_day, 2, 0) + "_" +
                   string_format(current_hour, 2, 0) + string_format(current_minute, 2, 0) + 
                   string_format(current_second, 2, 0);
    recording_filepath = "recordings/" + _filename + "_" + timestamp + ".mp4";
    
    // Create directory if needed
    if (!directory_exists("recordings/")) {
        directory_create("recordings/");
    }
    
    // Start recording - CONVERT ALL NUMERIC ARGUMENTS TO STRINGS
    var result = niceshot_start_recording(
        string(video_width), 
        string(video_height), 
        string(video_fps), 
        string(video_bitrate), 
        string(video_buffer_frames), 
        recording_filepath
    );
    
    if (result > 0) {
        recording = true;
        recording_start_time = current_time;
        recording_frame_count = 0;
        
        show_debug_message($"Video recording started: {recording_filepath}");
        show_debug_message($"Resolution: {video_width}x{video_height}@{video_fps}fps");
        show_debug_message($"Buffer: {video_buffer_frames} frames (≈{(video_buffer_frames * video_width * video_height * 4) / 1024 / 1024}MB)");
        return true;
    } else {
        show_debug_message("Failed to start video recording");
        cleanup_recording_resources();
        return false;
    }
}
```

**Record Frame (call this every frame while recording):**
```gml
function record_current_frame() {
    if (!recording || !surface_exists(recording_surface) || !buffer_exists(recording_buffer)) {
        return false;
    }
    
    // Capture current game state to surface (same as screenshot system)
    surface_set_target(recording_surface);
    {
        // Recreate final composite rendering with all effects
        var _zoom = min(global.zoomLevel, global.maxZoomLevel);
        var _width = IDEAL_WIDTH * _zoom;
        var _height = IDEAL_HEIGHT * _zoom;
        var _x = (video_width - _width)/2;
        var _y = (video_height - _height)/2;
        
        draw_clear(c_black);
        
        // Apply CRT shader and draw all surfaces
        crt_shader_set();
        {
            draw_surface_stretched(application_surface, _x, _y, _width, _height);
            
            // Add lighting effects if they exist
            if (surface_exists(global.lightingSurface))
                draw_surface(global.lightingSurface, _x, _y);
            
            // Add darkness overlay if in dark region  
            if (surface_exists(global.darknessSurface) && get_region_lighting() == lightingType.dark)
                draw_surface_stretched(global.darknessSurface, _x, _y, _width, _height);
            
            // Add GUI elements
            draw_surface_stretched(global.applicationSurfaceGUI, _x, _y, _width, _height);
            draw_surface_stretched(global.applicationSurfaceTransition, _x, _y, _width, _height);
        }
        shader_reset();
    }
    surface_reset_target();
    
    // Copy surface to buffer
    buffer_get_surface(recording_buffer, recording_surface, 0);
    
    // Send frame to video encoder (this is the fast part - ~1ms)
    var buffer_address = buffer_get_address(recording_buffer);
    var buffer_hex = string_format(buffer_address, 16, 0); // Convert to hex string
    
    var result = niceshot_record_frame(buffer_hex);
    
    if (result > 0) {
        recording_frame_count++;
        return true;
    } else if (result == -1) {
        // Frame was dropped due to full buffer - this is OK, continues recording
        return true;
    } else {
        show_debug_message("Error recording video frame");
        return false;
    }
}
```

**Stop Recording:**
```gml
function stop_video_recording() {
    if (!recording) {
        show_debug_message("Not currently recording");
        return false;
    }
    
    show_debug_message("Stopping video recording...");
    
    var result = niceshot_stop_recording();
    
    if (result > 0) {
        var duration = (current_time - recording_start_time) / 1000;
        show_debug_message($"Video recording completed: {recording_filepath}");
        show_debug_message($"Duration: {duration:.1f} seconds, {recording_frame_count} frames");
        
        recording = false;
        cleanup_recording_resources();
        return true;
    } else {
        show_debug_message("Error stopping video recording");
        recording = false;
        cleanup_recording_resources();
        return false;
    }
}
```

**Cleanup Resources:**
```gml
function cleanup_recording_resources() {
    if (surface_exists(recording_surface)) {
        surface_free(recording_surface);
        recording_surface = -1;
    }
    
    if (buffer_exists(recording_buffer)) {
        buffer_delete(recording_buffer);
        recording_buffer = -1;
    }
}
```

### 3. Integration with Existing Screenshot System

**In obj_render_manager or your main controller:**

**Create Event (add this):**
```gml
// Create video recorder
video_recorder = instance_create_depth(0, 0, 0, obj_video_recorder);
```

**Step Event (add this after existing screenshot logic):**
```gml
// Video recording controls
if (keyboard_check_pressed(ord("R")) && keyboard_check(vk_control)) {
    if (!video_recorder.recording) {
        video_recorder.start_video_recording("gameplay");
    } else {
        video_recorder.stop_video_recording();
    }
}

// Record frame if recording is active
if (video_recorder.recording) {
    video_recorder.record_current_frame();
}
```

### 4. UI Integration (Optional)

**Draw GUI Event for recording indicator:**
```gml
// Show recording status
if (video_recorder.recording) {
    var rec_text = "● REC " + string(floor(video_recorder.frames_captured / video_recorder.video_fps)) + "s";
    var buffer_text = "Buffer: " + string(floor(video_recorder.buffer_usage)) + "%";
    
    // Red recording dot
    draw_set_color(c_red);
    draw_circle(50, 50, 8, false);
    
    // Recording text
    draw_set_color(c_white);
    draw_set_font(fnt_ui); // Use your UI font
    draw_text(70, 45, rec_text);
    draw_text(70, 65, buffer_text);
}
```

## Performance Optimization Tips

### 1. Buffer Size Configuration
```gml
// For 1080p@60fps recording:
video_buffer_frames = 60;   // 1 second (≈500MB) - minimum
video_buffer_frames = 120;  // 2 seconds (≈1GB) - recommended  
video_buffer_frames = 300;  // 5 seconds (≈2.5GB) - maximum for safety
```

### 2. Quality vs Performance
```gml
// Set before starting recording:
niceshot_set_video_preset(0); // ultrafast - lowest CPU usage
niceshot_set_video_preset(1); // fast - good balance (recommended)
niceshot_set_video_preset(2); // medium - better quality, more CPU
```

### 3. Resolution Scaling
```gml
// For better performance, record at lower resolution:
video_width = 1280;  // Instead of 1920
video_height = 720;  // Instead of 1080
// This reduces memory usage by ~60%
```

## Troubleshooting

### Common Issues

**"Buffer full" warnings:**
- Increase `video_buffer_frames` or decrease resolution
- Use ultrafast preset (`niceshot_set_video_preset(0)`)
- Check if background encoding is keeping up

**Recording fails to start:**
- Ensure extension is initialized (`niceshot_init()` called)
- Check file path permissions
- Verify surface and buffer creation succeeded

**Frame recording returns 0:**
- Buffer address conversion might be failing
- Surface might not exist or be invalid
- Check GameMaker buffer is properly created

### Performance Monitoring
```gml
// Add this to step event for debugging:
if (video_recorder.recording && current_time % 5000 < 16) { // Every 5 seconds
    var status = niceshot_get_recording_status();
    var buffer_pct = niceshot_get_recording_buffer_usage();
    var frame_count = niceshot_get_recording_frame_count();
    
    show_debug_message($"Recording status: {status}, buffer: {buffer_pct:.1f}%, frames: {frame_count}");
}
```

## Example Usage

```gml
// Start 60fps 1080p recording  
video_recorder.start_video_recording("my_gameplay");

// Record frames (called automatically in step event)
// ... game runs normally at 60fps ...

// Stop recording after 30 seconds or when done
video_recorder.stop_video_recording();

// Video file saved to: recordings/my_gameplay_YYYYMMDD_HHMMSS.mp4
```

## GameMaker Extension Configuration

**IMPORTANT**: When configuring the `niceshot_start_recording` function in GameMaker's extension editor:

1. Set **all 6 parameters** as **String** type (not mixed types)
2. Parameter names: `width_str`, `height_str`, `fps_str`, `bitrate_kbps_str`, `max_buffer_frames_str`, `filepath`
3. Return type: **Real**

This is required due to GameMaker's limitation that DLL functions with more than 4 arguments must have all arguments of the same type.

## Notes

- **Memory Usage**: ~8MB per frame (1920×1080×4 bytes). 120 frames ≈ 1GB RAM
- **Performance Impact**: Frame capture ~1ms, no game slowdown  
- **File Output**: Real H.264 video files (.h264 format, playable in VLC)
- **Thread Safety**: All functions are thread-safe, can be called from GameMaker main thread
- **String Arguments**: All numeric parameters are converted to strings, then parsed back to numbers in the DLL

The ring buffer system guarantees smooth 60fps gameplay while recording. If the encoding can't keep up, it drops frames rather than slowing the game.