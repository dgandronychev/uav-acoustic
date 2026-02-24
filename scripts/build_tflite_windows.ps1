$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path "$PSScriptRoot/..").Path
$ConanOut = Join-Path $RootDir "out/conan"
$BuildDir = Join-Path $RootDir "out/build/tflite-release"

$Generator = $env:UAV_CMAKE_GENERATOR
if (-not $Generator) {
  $Generator = "VS"
}

switch -Regex ($Generator) {
  "^(VS|VisualStudio|Visual Studio)$" { $Generator = "Visual Studio 17 2022"; break }
  default { }
}

conan install $RootDir --output-folder $ConanOut --build=missing `
  -s:h compiler.cppstd=17 `
  -s:b compiler.cppstd=17

$ToolchainFile = Join-Path $ConanOut "conan_toolchain.cmake"
if (-not (Test-Path $ToolchainFile)) {
  $ToolchainFile = Join-Path $ConanOut "build/generators/conan_toolchain.cmake"
}
if (-not (Test-Path $ToolchainFile)) {
  throw "Conan toolchain not found in $ConanOut or $ConanOut/build/generators"
}

$CMakeArgs = @(
  "-S", $RootDir,
  "-B", $BuildDir,
  "-G", $Generator,
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile",
  "-DUAV_ENABLE_TFLITE=ON"
)

if ($Generator -eq "Ninja") {
  if (Test-Path Env:CMAKE_GENERATOR_PLATFORM) {
    Remove-Item Env:CMAKE_GENERATOR_PLATFORM
  }
  $env:CMAKE_GENERATOR_PLATFORM = ""
  $CMakeArgs += "-DCMAKE_GENERATOR_PLATFORM="

  $CacheFile = Join-Path $BuildDir "CMakeCache.txt"
  if (Test-Path $CacheFile) {
    $CacheContents = Get-Content $CacheFile -Raw
    if ($CacheContents -match "CMAKE_GENERATOR_PLATFORM") {
      Remove-Item $CacheFile
    }
  }
}

cmake @CMakeArgs

cmake --build $BuildDir
