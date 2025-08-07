# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NiceShot is a C++ GameMaker Studio extension (DLL) for asynchronous PNG screenshot and video frame capturing. The extension eliminates frame drops during image saving by using multi-threaded PNG encoding in the background.

**Current Status**: Basic GameMaker extension working with stub PNG functionality. Ready for libpng integration.

## Build Commands

### Visual Studio Build
```bash
# Build Debug version
msbuild NiceShot.vcxproj /p:Configuration=Debug /p:Platform=x64

# Build Release version  
msbuild NiceShot.vcxproj /p:Configuration=Release /p:Platform=x64
```

### Command Line Build (using Developer Command Prompt)
```bash
# Navigate to project directory
cd /d C:\Users\Daniel\source\repos\nice-shot

# Clean and rebuild
msbuild NiceShot.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Clean,Build
```

### Build Outputs
- **Debug DLL**: `bin/Debug/NiceShot.dll`
- **Release DLL**: `bin/Release/NiceShot.dll`

## Project Architecture

### Core Components

1. **GameMaker Interface** (`src/niceshot.h`, `src/niceshot.cpp`)
   - C interface functions for GameMaker integration
   - Current functions: `niceshot_init()`, `niceshot_shutdown()`, `niceshot_test()`, `niceshot_save_png()`
   - All functions use `extern "C"` to prevent C++ name mangling

2. **Visual Studio Project** (`NiceShot.vcxproj`)
   - Configured for x64 DynamicLibrary (.dll) output
   - C++17 standard, Visual Studio 2022 toolset
   - Dependencies: libpng16.lib, zlib.lib (currently missing - causes LNK1104 error)

3. **Planned Architecture** (from docs/async_png_extension_plan.md)
   - **Thread Pool**: Multi-threaded PNG encoding
   - **PNG Encoder**: libpng wrapper with error handling
   - **Job Manager**: Async save operation tracking
   - **GameMaker Wrapper**: GML functions for easy integration

### Current Implementation Status

**Working**:
- Basic DLL structure and GameMaker communication
- Extension loads successfully in GameMaker
- Test functions work correctly (verified with ProtoDungeon3 integration)

**Missing**:
- libpng/zlib libraries causing build error LNK1104
- Actual PNG encoding implementation (currently stubbed)
- Threading system for async operations
- Job management for tracking save operations

## Dependencies

### Required Libraries (Currently Missing)
- **libpng16.lib** - PNG encoding/decoding library
- **zlib.lib** - Compression library required by libpng

### Build Error Resolution
The current build error `LNK1104 cannot open file 'libpng16.lib'` indicates missing library dependencies. To resolve:

1. Download libpng Windows binaries
2. Download zlib Windows binaries  
3. Update vcxproj with correct library paths
4. Ensure libraries are available at build time

## Development Workflow

### Testing Extension Changes
1. Build DLL with Visual Studio or msbuild
2. Copy output DLL to GameMaker extension directory
3. Test in GameMaker project (currently ProtoDungeon3)
4. Verify functions work through GameMaker's extension system

### Next Implementation Steps (Priority Order)
1. **Fix Dependencies**: Add libpng/zlib libraries to resolve build error
2. **Implement PNG Encoding**: Replace stubbed `niceshot_save_png()` with actual libpng calls
3. **Add Threading**: Implement background PNG encoding with thread pool
4. **Add Async Interface**: Create job-based async PNG save functions
5. **GameMaker Integration**: Update extension interface for production use

## GameMaker Extension Integration

### Extension Functions (Current)
- `niceshot_init()` - Initialize extension (returns 1.0 on success)
- `niceshot_shutdown()` - Cleanup extension
- `niceshot_test(input)` - Connection test (returns input + 1)
- `niceshot_get_version()` - Version string
- `niceshot_save_png(buffer_ptr, width, height, filepath)` - PNG save (stubbed)

### Planned Async Functions
- `async_png_save_buffer()` - Start async PNG save, returns job_id
- `async_png_is_complete(job_id)` - Check if save complete
- `async_png_get_status(job_id)` - Get detailed job status
- `async_png_cleanup_job(job_id)` - Free completed job resources

## Code Conventions

- **Language**: C++17
- **Platform**: Windows x64 (Phase 1), cross-platform planned
- **Interface**: C functions with `extern "C"` for GameMaker compatibility
- **Error Handling**: Functions return 1.0 for success, 0.0 for failure
- **Logging**: std::cout for info, std::cerr for errors (prefixed with [NiceShot])
- **Memory**: Manual memory management, careful buffer handling from GameMaker

## Key File Locations

- **Source**: `src/niceshot.h`, `src/niceshot.cpp`
- **Project**: `NiceShot.vcxproj`, `NiceShot.sln`
- **Output**: `bin/Debug/` or `bin/Release/`
- **Documentation**: `docs/async_png_extension_plan.md` (comprehensive implementation plan)
- **Build Files**: `obj/Debug/` (excluded from git)

## Testing Notes

Extension successfully tested with GameMaker Studio 2 integration:
- DLL loads correctly
- Functions callable from GML
- Test output shows proper communication
- Ready for PNG encoding implementation

## Performance Goals

- **Frame Rate**: Maintain 60 FPS during screenshot/recording
- **Threading**: 2-4 background threads for PNG encoding
- **Memory**: Efficient buffer management, auto-cleanup completed jobs
- **Disk I/O**: Distributed saves to prevent I/O bottlenecks