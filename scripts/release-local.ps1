# The release workflow's windows job, run on this machine - for when
# GitHub Actions cannot run (.github/workflows/release.yml is the
# reference; keep the two in step).
#
#   pwsh scripts/release-local.ps1 -Version 0.4.0 -Dist D:\dist [-Work D:\relwin]
#
# Builds from the commit the tag v<Version> names, with MSVC: the library
# against the DLL runtime, Doctor again against the static one, so the
# shipped doctor.exe needs nothing beside it. Each build is tested first.
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$Dist,
    [string]$Work = (Join-Path ([IO.Path]::GetTempPath()) "configforge-release")
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true
$repo = Split-Path -Parent $PSScriptRoot
$v = $Version

git -C $repo rev-parse -q --verify "refs/tags/v$v" | Out-Null
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Force $Work, $Dist | Out-Null
$Dist = (Resolve-Path $Dist).Path
git -C $repo archive --format=zip -o "$Work\src.zip" "v$v"
Expand-Archive "$Work\src.zip" -DestinationPath "$Work\src"
Set-Location "$Work\src"

cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$PWD/stage/configforge-$v" | Out-Null
cmake --build build --config Release -- /m:4 /v:quiet /nologo | Out-Null
ctest --test-dir build -C Release --output-on-failure | Select-Object -Last 3
cmake --install build --config Release | Out-Null
cmake -S . -B build-doctor -DCMAKE_POLICY_DEFAULT_CMP0091=NEW `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded | Out-Null
cmake --build build-doctor --config Release --target doctor doctor_fixtures -- /m:4 /v:quiet /nologo | Out-Null
ctest --test-dir build-doctor -C Release -R "^doctor\." --output-on-failure | Select-Object -Last 3
New-Item -ItemType Directory -Force "stage/configforge-$v/bin" | Out-Null
Copy-Item build-doctor/tools/doctor/Release/doctor.exe "stage/configforge-$v/bin/"
Compress-Archive -Force -Path "stage/configforge-$v" -DestinationPath "$Dist/configforge-$v-windows-x64.zip"
Get-Item "$Dist/configforge-$v-windows-x64.zip" | ForEach-Object { "{0}: {1} bytes" -f $_.Name, $_.Length }
