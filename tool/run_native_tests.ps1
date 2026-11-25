# Builds the native core unit tests with the Android NDK and runs them on a connected device.
# Usage: .\tool\run_native_tests.ps1 [-Device <adb serial>] [-Abi arm64-v8a|x86_64]
param(
  [string]$Device = "",
  [string]$Abi = "arm64-v8a"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA "Android\sdk" }
$ndk = Get-ChildItem (Join-Path $sdk "ndk") | Sort-Object Name -Descending | Select-Object -First 1
$cmakeDir = Get-ChildItem (Join-Path $sdk "cmake") | Sort-Object Name -Descending | Select-Object -First 1
$cmake = Join-Path $cmakeDir.FullName "bin\cmake.exe"
$ninja = Join-Path $cmakeDir.FullName "bin\ninja.exe"
$adb = Join-Path $sdk "platform-tools\adb.exe"
$build = Join-Path $root "build\native_tests\$Abi"
$toolchain = Join-Path $ndk.FullName "build\cmake\android.toolchain.cmake"

& $cmake -G Ninja -S (Join-Path $root "android\app\src\main\cpp") -B $build `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DANDROID_ABI=$Abi" "-DANDROID_PLATFORM=android-24" `
  "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_BUILD_TYPE=Release" "-DLIDARCORE_TESTS=ON" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
& $cmake --build $build --target lidarcore_tests
if ($LASTEXITCODE -ne 0) { throw "native build failed" }

$ErrorActionPreference = "Continue"
$adbArgs = @()
if ($Device) { $adbArgs = @("-s", $Device) }
& $adb @adbArgs push (Join-Path $build "lidarcore_tests") /data/local/tmp/lidarcore_tests 2>&1 | Out-Null
& $adb @adbArgs shell "chmod +x /data/local/tmp/lidarcore_tests && /data/local/tmp/lidarcore_tests"
exit $LASTEXITCODE
