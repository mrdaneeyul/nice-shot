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
}