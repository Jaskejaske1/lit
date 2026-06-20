# build.ps1
$ErrorActionPreference = "Stop"

# 1. Clean up the root directory from previous sloppy builds
Remove-Item *.obj, *.exp, *.lib, lit.exe, SDL3.dll, imgui.ini -ErrorAction SilentlyContinue

# 2. Setup the isolated build environment
New-Item -ItemType Directory -Force build/obj | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio with C++ workload not found." }

$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"

Write-Host "Compiling project lit into /build..." -ForegroundColor Green

$srcFiles = Get-ChildItem -Path src -Filter *.cpp -Recurse | ForEach-Object { $_.FullName }
$srcList = $srcFiles -join " "

# 3. Compile: Added /Zi (Debug info), /Od (No optimization), and /DEBUG linker flag
$buildArgs = "cl.exe /nologo /std:c++20 /Od /Zi /EHsc /DIMGUI_IMPL_OPENGL_LOADER_GL3W " +
             "/Ideps/imgui /Ideps/imgui/backends /Ideps/gl3w/include /Ideps/SDL3/include /Isrc " +
             "$srcList deps/gl3w/src/gl3w.c " +
             "deps/imgui/imgui.cpp deps/imgui/imgui_draw.cpp deps/imgui/imgui_widgets.cpp deps/imgui/imgui_tables.cpp " +
             "deps/imgui/backends/imgui_impl_sdl3.cpp deps/imgui/backends/imgui_impl_opengl3.cpp " +
             "/Fd`"build/obj/`" /Fo`"build/obj/`" /Fe`"build/lit.exe`" " +
             "/link /DEBUG /LIBPATH:deps/SDL3/lib/x64 SDL3.lib opengl32.lib user32.lib gdi32.lib shell32.lib"

cmd.exe /c "`"$vcvars`" >NUL && $buildArgs"

if ($LastExitCode -eq 0) {
    Write-Host "Build successful. Launching..." -ForegroundColor Cyan
    Copy-Item "deps/SDL3/lib/x64/SDL3.dll" "build/SDL3.dll" -ErrorAction SilentlyContinue
} else {
    Write-Host "Build failed." -ForegroundColor Red
}