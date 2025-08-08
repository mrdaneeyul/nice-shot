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

**✅ Working**:
- Basic DLL structure and GameMaker communication
- Extension loads successfully in GameMaker
- Test functions work correctly (verified with ProtoDungeon3 integration)
- libpng/zlib dependencies resolved via vcpkg
- Real PNG encoding implementation with libpng
- PNG saving function accepts buffer pointer, width, height, and filepath
- Comprehensive error handling for PNG operations
- **GameMaker integration tested and confirmed working**:
  - Surface screenshots (1280x720) working perfectly
  - Composite screenshots (1600x900) with effects working
  - Buffer parsing and memory validation implemented
  - 2/3 tests passing (test function fixed with direct PNG encoding)

**🚧 Next Phase**:
- Threading system for async operations
- Job management for tracking save operations

## Async Threading System - Implementation Plan

### Architecture Overview
```
GameMaker Thread          Worker Thread
     │                         │
     ├─ niceshot_save_png_async(buffer, width, height, filepath)
     │  ├─ Copy buffer data    │
     │  ├─ Create job          │
     │  ├─ Queue job ────────► │ ├─ Process job queue
     │  └─ Return job_id       │ ├─ Encode PNG
     │                         │ ├─ Save to file
     │                         │ └─ Mark job complete
     ├─ niceshot_get_job_status(job_id)
     └─ niceshot_cleanup_job(job_id)
```

### Implementation Steps
1. **Job Queue System**
   - Thread-safe job queue using std::queue + std::mutex
   - Job structure: id, buffer_data, width, height, filepath, status
   - Job status tracking: QUEUED, PROCESSING, COMPLETED, FAILED

2. **Worker Thread**
   - Background std::thread processing job queue
   - PNG encoding using existing libpng implementation
   - Safe memory management with copied buffer data

3. **Job Management**
   - Unique job ID generation
   - Status checking functions for GameMaker
   - Job cleanup and memory management

4. **Thread Safety**
   - Mutex protection for shared data structures
   - Condition variables for efficient thread communication
   - RAII for automatic resource cleanup

### New Functions to Implement
- `niceshot_save_png_async(buffer_ptr_str, width, height, filepath)` → returns job_id
- `niceshot_get_job_status(job_id)` → returns status (0=queued, 1=processing, 2=complete, -1=failed)
- `niceshot_cleanup_job(job_id)` → cleanup completed job
- `niceshot_get_pending_job_count()` → number of jobs in queue
- `niceshot_worker_thread_status()` → worker thread health check

## Dependencies

### ✅ Resolved Libraries
- **libpng:x64-windows** - PNG encoding/decoding library (via vcpkg)
- **zlib:x64-windows** - Compression library required by libpng (via vcpkg)
- **vcpkg integration** - Automatic library linking and DLL deployment

### vcpkg Configuration
The project uses vcpkg for dependency management:
```bash
vcpkg install libpng:x64-windows zlib:x64-windows
vcpkg integrate install
```
- Libraries automatically linked (no manual .lib references needed)
- DLLs automatically copied to output directory (libpng16.dll, zlib1.dll)

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
- ✅ `niceshot_save_png(buffer_ptr, width, height, filepath)` - PNG save (implemented with libpng)
- ✅ `niceshot_test_png()` - Creates test 100x100 gradient PNG for verification

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