param([ValidateSet('Release','Debug')][string]$Configuration = 'Release', [switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'build'
$sdkRoot = (Join-Path $projectRoot '.cache/dxsdk/build/native').Replace('\','/')
$engine = if ($CoreOnly) { 'OFF' } else { 'ON' }
& cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A Win32 "-DCMAKE_BUILD_TYPE=$Configuration" "-DCOD4LIVE_BUILD_ENGINE=$engine" '-DCICD=ON' "-DDXSDK_DIR=$sdkRoot" '-DTRACY_ENABLE=OFF' '-DBUILD_TESTING=ON'
if ($LASTEXITCODE) { throw 'CMake configuration failed' }
& cmake --build $buildRoot --config $Configuration --parallel 8
if ($LASTEXITCODE) { throw 'Build failed' }
& ctest --test-dir $buildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
