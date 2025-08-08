# VCPKG Setup Instructions - Fix Build Errors

The build errors indicate that the vcpkg dependencies (png.h, zlib.h, x264.h) aren't being found. Here's how to fix this:

## Step 1: Install/Setup vcpkg

### If vcpkg is NOT installed:
```bash
# Open Command Prompt as Administrator
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### If vcpkg IS installed:
```bash
# Find your vcpkg installation (usually C:\vcpkg)
cd C:\vcpkg
# Or wherever you have vcpkg installed
```

## Step 2: Install Required Packages

```bash
# From your vcpkg directory (e.g., C:\vcpkg)
.\vcpkg install libpng:x64-windows-static
.\vcpkg install zlib:x64-windows-static
.\vcpkg install x264:x64-windows-static

# Verify installation
.\vcpkg list | findstr "libpng\|zlib\|x264"
```

Expected output:
```
libpng[core]:x64-windows-static        1.6.43#1
x264[core]:x64-windows-static          164-stable
zlib[core]:x64-windows-static          1.3.1
```

## Step 3: Verify vcpkg Integration

```bash
# From vcpkg directory
.\vcpkg integrate show
```

Should show something like:
```
CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
MSBuild projects should use: (requires v143 toolset)
    C:\vcpkg\scripts\buildsystems\msbuild\vcpkg.targets
```

## Step 4: Fix Project Configuration

### Option A: Automatic (if vcpkg integrate install worked)
Your project should automatically find the libraries. Try building again.

### Option B: Manual Library Paths (if Option A fails)

Edit your `NiceShot.vcxproj` file and add explicit include/library paths:

```xml
<!-- Add this after existing PropertyGroup sections -->
<PropertyGroup>
  <VcpkgRoot Condition="'$(VcpkgRoot)'==''">C:\vcpkg</VcpkgRoot>
  <IncludePath>$(VcpkgRoot)\installed\x64-windows-static\include;$(IncludePath)</IncludePath>
  <LibraryPath>$(VcpkgRoot)\installed\x64-windows-static\lib;$(LibraryPath)</LibraryPath>
</PropertyGroup>
```

And add explicit library dependencies to the Release configuration:

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
  <Link>
    <!-- Existing link settings... -->
    <AdditionalDependencies>
      libpng16.lib;
      zlib.lib;
      x264.lib;
      kernel32.lib;
      user32.lib;
      %(AdditionalDependencies)
    </AdditionalDependencies>
  </Link>
</ItemDefinitionGroup>
```

## Step 5: Alternative - Download Pre-built Libraries

If vcpkg continues to have issues, you can download pre-built libraries:

### Download Links:
1. **libpng**: https://github.com/glennrp/libpng/releases (Windows binaries)
2. **zlib**: https://zlib.net/ (Windows binaries)  
3. **x264**: https://github.com/ShiftMediaProject/x264/releases (MSVC builds)

### Manual Setup:
1. Create `libs/` folder in your project directory
2. Extract libraries to `libs/libpng/`, `libs/zlib/`, `libs/x264/`
3. Update project file with manual paths:

```xml
<PropertyGroup>
  <IncludePath>$(SolutionDir)libs\libpng\include;$(SolutionDir)libs\zlib\include;$(SolutionDir)libs\x264\include;$(IncludePath)</IncludePath>
  <LibraryPath>$(SolutionDir)libs\libpng\lib;$(SolutionDir)libs\zlib\lib;$(SolutionDir)libs\x264\lib;$(LibraryPath)</LibraryPath>
</PropertyGroup>
```

## Step 6: Test the Build

After setting up the dependencies:

1. **Clean the project**: Build → Clean Solution
2. **Rebuild**: Build → Rebuild Solution  
3. **Check output**: Should show successful linking

## Common Issues & Solutions

### Issue: "vcpkg integrate install failed"
**Solution**: Run Command Prompt as Administrator

### Issue: "x264 package not found"
**Solution**: 
```bash
.\vcpkg search x264
# If not found, update vcpkg:
git pull
.\vcpkg integrate install
```

### Issue: "LNK2019 unresolved external symbol"
**Solution**: Ensure you're using the static triplet and /MT runtime:
- Project Properties → C/C++ → Code Generation → Runtime Library = Multi-threaded (/MT)

### Issue: Still can't find headers
**Solution**: Check exact paths:
```bash
dir "C:\vcpkg\installed\x64-windows-static\include"
# Should show png.h, x264.h, etc.
```

## Verification Commands

After setup, verify with:
```bash
# Check vcpkg integration
vcpkg integrate show

# List installed packages  
vcpkg list | findstr "x64-windows-static"

# Check include directory
dir "C:\vcpkg\installed\x64-windows-static\include" | findstr "png\|x264"
```

Once you see `png.h`, `x264.h` in the include directory, the build should work.

## Quick Test

Create a simple test to verify libraries are working:

**test.cpp**:
```cpp
#include <png.h>
extern "C" {
#include <x264.h>
}
#include <iostream>

int main() {
    std::cout << "PNG version: " << PNG_LIBPNG_VER_STRING << std::endl;
    std::cout << "x264 build: " << x264_build << std::endl;
    return 0;
}
```

If this compiles and runs, your libraries are set up correctly.