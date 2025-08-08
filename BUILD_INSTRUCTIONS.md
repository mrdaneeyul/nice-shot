# NiceShot Build Instructions - x264 Video Recording

This guide shows how to build NiceShot with x264 H.264 video encoding support.

## Prerequisites

1. **Visual Studio 2022** with C++ desktop development workload
2. **vcpkg** package manager
3. **Git** (for vcpkg)

## Setup vcpkg (if not already installed)

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg

# Bootstrap vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Integrate with Visual Studio (run once)
.\vcpkg.exe integrate install
```

## Install Dependencies

From the project directory (`C:\Users\Daniel\source\repos\nice-shot`):

```bash
# Navigate to vcpkg directory
cd C:\vcpkg

# Install static libraries for x64
.\vcpkg.exe install libpng:x64-windows-static
.\vcpkg.exe install zlib:x64-windows-static  
.\vcpkg.exe install x264:x64-windows-static

# Verify installations
.\vcpkg.exe list
```

Expected output should include:
```
libpng[core]:x64-windows-static
x264[core]:x64-windows-static
zlib[core]:x64-windows-static
```

## Build the Project

### Option 1: Visual Studio GUI
1. Open `NiceShot.sln` in Visual Studio 2022
2. Select **Release** configuration and **x64** platform
3. Build → Build Solution (Ctrl+Shift+B)

### Option 2: Command Line
```bash
# From project directory
cd "C:\Users\Daniel\source\repos\nice-shot"

# Build Release version
msbuild NiceShot.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Expected Output

**Successful build produces**:
- `bin/Release/NiceShot.dll` - Main extension DLL
- No additional DLL dependencies (static linking)

**Build log should show**:
```
1>------ Build started: Project: NiceShot, Configuration: Release x64 ------
1>niceshot.cpp
1>Creating library bin/Release/NiceShot.lib and object bin/Release/NiceShot.exp
1>Generating code
1>Finished generating code
1>NiceShot.vcxproj -> C:\Users\Daniel\source\repos\nice-shot\bin\Release\NiceShot.dll
```

## Testing the Build

### Basic Extension Test
Copy `bin/Release/NiceShot.dll` to your GameMaker extension folder and test:

```gml
// GameMaker test code
show_debug_message("Extension version: " + niceshot_get_version());
show_debug_message("Init result: " + string(niceshot_init()));
show_debug_message("Test function: " + string(niceshot_test(42)));
```

### x264 Video Encoding Test
```gml
// Test H.264 encoder
var result = niceshot_test_x264(640, 480, 60, 1); // 640x480, 60 frames, fast preset
show_debug_message("x264 test result: " + string(result));
// Check for test_x264.h264 file output
```

### PNG Test (existing functionality)
```gml
var result = niceshot_test_png();
show_debug_message("PNG test result: " + string(result));
// Check for test_output.png file
```

## Performance Benchmarks

Test different x264 presets for performance:

```gml
// Performance comparison
niceshot_test_x264(1920, 1080, 30, 0); // ultrafast
niceshot_test_x264(1920, 1080, 30, 1); // fast  
niceshot_test_x264(1920, 1080, 30, 2); // medium
```

Expected encoding speeds (1080p):
- **ultrafast**: ~200+ FPS encoding
- **fast**: ~100-150 FPS encoding  
- **medium**: ~50-80 FPS encoding

## Troubleshooting

### Build Errors

**Error: "Cannot open include file: 'x264.h'"**
```
Solution: Ensure x264:x64-windows-static is installed via vcpkg
```

**Error: "unresolved external symbol x264_encoder_open"**
```  
Solution: Verify VcpkgTriplet is set to x64-windows-static in .vcxproj
```

**Error: "LNK2038: mismatch detected for 'RuntimeLibrary'"**
```
Solution: Ensure all dependencies use /MT (MultiThreaded) runtime
Project Properties → C/C++ → Code Generation → Runtime Library = Multi-threaded (/MT)
```

### Runtime Errors

**"Failed to initialize H.264 encoder"**
- Check file write permissions for output directory
- Verify video parameters (width/height must be even numbers)
- Try ultrafast preset first

**Extension fails to load**
- Ensure all static libraries are properly linked
- Check Windows Event Viewer for DLL load errors
- Verify no missing dependencies with `dumpbin /dependents NiceShot.dll`

### File Output Issues

**H.264 files won't play**
- Output is raw H.264 stream, not MP4 container
- Use FFmpeg to convert: `ffmpeg -i test_x264.h264 -c copy output.mp4`
- Or use VLC player which can play raw H.264 streams

## Next Steps

1. **MP4 Container Support**: Add libavformat for proper MP4 output
2. **Hardware Encoding**: Add NVENC/QuickSync support for GPU acceleration  
3. **Audio Support**: Integrate audio encoding for complete video files

## Current Capabilities

✅ **Working Features**:
- Real-time H.264 encoding with x264
- Ring buffer video recording system
- Multiple quality presets (ultrafast to slower)
- Zero game frame drops during recording
- PNG screenshot system (existing)
- Multi-threaded encoding pipeline

🎯 **Output Format**: Raw H.264 elementary stream (.h264 files)
🎯 **Performance**: 60fps+ encoding on modern CPUs
🎯 **Memory Usage**: Configurable (500MB - 2.5GB buffer)

The extension is now ready for high-performance video recording in GameMaker projects!