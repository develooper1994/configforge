# The development environment as a WSL distribution of its own, on a
# drive of your choosing - for a Windows machine where neither a
# container runtime nor C: has room for it.
#
#   pwsh scripts/buildenv/wsl-distro.ps1 -Dir D:\cf-wsl
#   wsl -d configforge-dev -- cmake -DSOURCE=index -DWORK=/root/cfv -P scripts/verify.cmake
#
# It is made from Canonical's Ubuntu 24.04 base image, checked against
# the published SHA256SUMS, and provisioned by setup.sh, the same script
# the Containerfile runs. Its disk lives in -Dir, so the builds inside it
# grow that drive and nothing else. You are root inside it; it shares
# nothing with any other distribution. To remove it:
#
#   wsl --unregister configforge-dev
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [string]$Name = "configforge-dev",
    [string]$Release = "24.04.5"
)
$ErrorActionPreference = "Stop"

$base    = "https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release"
$tarball = Join-Path $Dir "ubuntu-base.tar.gz"
$sums    = Join-Path $Dir "SHA256SUMS"
New-Item -ItemType Directory -Force $Dir | Out-Null

if (-not (Test-Path $tarball)) {
    Invoke-WebRequest -UseBasicParsing "$base/SHA256SUMS" -OutFile $sums
    Invoke-WebRequest -UseBasicParsing "$base/ubuntu-base-$Release-base-amd64.tar.gz" -OutFile $tarball
}
$want = ((Get-Content $sums | Select-String "ubuntu-base-$Release-base-amd64.tar.gz").Line -split " ")[0]
$got  = (Get-FileHash $tarball -Algorithm SHA256).Hash.ToLower()
if ($want -ne $got) { throw "checksum mismatch for ${tarball}: want $want, got $got" }

if (-not ((wsl -l -q) -replace "`0", "" | Where-Object { $_ -eq $Name })) {
    wsl --import $Name (Join-Path $Dir $Name) $tarball --version 2
}
$setup = (Resolve-Path (Join-Path $PSScriptRoot "setup.sh")).Path
$wslSetup = wsl -d $Name -- wslpath -a ($setup -replace "\\", "/")
wsl -d $Name -- sh $wslSetup
Write-Output "ready: wsl -d $Name"
