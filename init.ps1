# init.ps1
$ErrorActionPreference = "Stop"

Remove-Item -Recurse -Force deps, out, lit.exe, lit.obj, *.pdb, SDL3.dll -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force deps | Out-Null

Write-Host "Fetching SDL3..." -ForegroundColor Green
$sdlVersion = "3.2.0"
$sdlUrl = "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL3-devel-$sdlVersion-VC.zip"
Invoke-WebRequest -Uri $sdlUrl -OutFile "deps/sdl3.zip"

Write-Host "Extracting SDL3..." -ForegroundColor Green
Expand-Archive -Path "deps/sdl3.zip" -DestinationPath "deps/tmp_sdl"

$extractedFolder = Get-ChildItem "deps/tmp_sdl" | Where-Object { $_.PSIsContainer } | Select-Object -First 1
if ($null -ne $extractedFolder) {
    Move-Item $extractedFolder.FullName "deps/SDL3"
} else {
    Throw "Error: No directory found inside the extracted SDL3 zip file."
}

Remove-Item -Recurse -Force deps/tmp_sdl, deps/sdl3.zip

Write-Host "Cloning Dear ImGui..." -ForegroundColor Green
git clone --depth 1 https://github.com/ocornut/imgui.git deps/imgui

Write-Host "Generating OpenGL 4.6 core headers via GL3W..." -ForegroundColor Green
git clone --depth 1 https://github.com/skaslev/gl3w.git deps/gl3w
Push-Location deps/gl3w
python gl3w_gen.py
Pop-Location

Write-Host "--- SETUP SUCCESSFUL ---" -ForegroundColor Cyan