# The development environment on Windows, in one command: the native side
# (MSVC, CMake, Ninja, Git, Python, LLVM) through winget, and the Linux
# side as the WSL distribution wsl-distro.ps1 makes, on the drive you name.
#
#   pwsh scripts/buildenv/dev-setup.ps1 -WslDir D:\cf-wsl   install what is missing
#   pwsh scripts/buildenv/dev-setup.ps1 -Check              say what is here, change nothing
#   pwsh scripts/buildenv/dev-setup.ps1 -NoWsl              the native side only
#
# Run again at any time: what is installed is skipped, and an existing
# distribution is brought up to setup.sh's current list. winget asks
# before it installs anything that needs an administrator.
param(
    [string]$WslDir,
    [string]$Name = "configforge-dev",
    [switch]$Check,
    [switch]$NoWsl
)
$ErrorActionPreference = "Stop"

# command, winget id, what it is for
$tools = @(
    @("cmake", "Kitware.CMake", "every build"),
    @("ninja", "Ninja-build.Ninja", "fast builds"),
    @("git", "Git.Git", "SOURCE=index, release"),
    @("python", "Python.Python.3.12", "gates, amalgamation, Python binding"),
    @("clang-format", "LLVM.LLVM", "gate.format")
)
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

function Test-Msvc {
    (Test-Path $vswhere) -and
    (& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
}
function Test-Distro {
    [bool]((wsl -l -q) -replace "`0", "" | Where-Object { $_ -eq $Name })
}
function Show-State {
    $missing = 0
    foreach ($t in $tools) {
        $ok = [bool](Get-Command $t[0] -ErrorAction SilentlyContinue)
        if (-not $ok) { $missing++ }
        "  {0,-8} {1,-16} {2}" -f ($(if ($ok) { "ok" } else { "missing" })), $t[0], $t[2]
    }
    $ok = [bool](Test-Msvc)
    if (-not $ok) { $missing++ }
    "  {0,-8} {1,-16} {2}" -f ($(if ($ok) { "ok" } else { "missing" })), "MSVC", "the Windows build and tests"
    if (-not $NoWsl) {
        $ok = Test-Distro
        if (-not $ok) { $missing++ }
        "  {0,-8} {1,-16} {2}" -f ($(if ($ok) { "ok" } else { "missing" })), $Name, "the Linux side: every EXTRA step"
    }
    if ($missing -eq 0) { "everything is here" } else { "$missing missing" }
}

if ($Check) { Show-State; return }

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget is needed (App Installer, from the Microsoft Store)"
}
foreach ($t in $tools) {
    if (-not (Get-Command $t[0] -ErrorAction SilentlyContinue)) {
        winget install --id $t[1] --exact --source winget
    }
}
if (-not (Test-Msvc)) {
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget `
        --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
}

if (-not $NoWsl) {
    if (Test-Distro) {
        # Already made: bring it up to the current list.
        $setup = (Resolve-Path (Join-Path $PSScriptRoot "setup.sh")).Path
        $wslSetup = wsl -d $Name -- wslpath -a ($setup -replace "\\", "/")
        wsl -d $Name -- sh $wslSetup
    } elseif ($WslDir) {
        & (Join-Path $PSScriptRoot "wsl-distro.ps1") -Dir $WslDir -Name $Name
    } else {
        throw "-WslDir is needed to make ${Name}: a drive with room, such as D:\cf-wsl"
    }
}
"Open a new terminal so the new PATH applies, then:"
Show-State
