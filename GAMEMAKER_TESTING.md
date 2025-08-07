# NiceShot PNG Testing Guide for GameMaker

## Current Extension Functions Available

```gml
// Test function - creates a 100x100 gradient PNG
niceshot_test_png()  // Returns 1.0 on success, 0.0 on failure

// Real PNG save function  
niceshot_save_png(buffer_ptr, width, height, filepath)
// - buffer_ptr: GameMaker buffer address (use buffer_get_address())
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
    
    // Create buffer from surface
    var buffer = buffer_create(surf_w * surf_h * 4, buffer_fixed, 1);
    buffer_get_surface(buffer, application_surface, 0);
    
    // Save as PNG using our extension
    var save_result = niceshot_save_png(
        buffer_get_address(buffer), 
        surf_w, 
        surf_h, 
        "gamemaker_screenshot.png"
    );
    
    // Cleanup
    buffer_delete(buffer);
    
    if (save_result > 0) {
        show_debug_message("SUCCESS: gamemaker_screenshot.png saved!");
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
    
    var result = niceshot_save_png(
        buffer_get_address(buffer),
        surf_w,
        surf_h,
        filepath  // Your existing filepath variable
    );
    
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

## Next Steps After Testing

Once PNG functionality is verified:
1. Implement async threading system for frame-drop-free recording
2. Add job management for tracking multiple PNG saves
3. Create high-performance frame-by-frame video recording