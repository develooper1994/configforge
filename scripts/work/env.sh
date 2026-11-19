# shellcheck shell=sh disable=SC2034 # `here` and cf_win are read by the scripts that source this
# Sourced by the scripts beside it: where they work and how they reach
# Linux. Every value can be set from outside.
#   CF_WORK    a directory with room for copies and Windows builds
#              (default D:/cf-work when D: exists, else $TMPDIR/cf-work)
#   CF_DISTRO  the WSL distribution with setup.sh's tools (configforge-dev)
here=$(cd "$(dirname "$0")" && pwd)
case "$(uname -s)" in MINGW* | MSYS* | CYGWIN*) cf_win=1 ;; *) cf_win=0 ;; esac
if [ -z "$CF_WORK" ]; then
  if [ "$cf_win" = 1 ] && [ -d /d ]; then CF_WORK=/d/cf-work; else CF_WORK=${TMPDIR:-/tmp}/cf-work; fi
fi
CF_DISTRO=${CF_DISTRO:-configforge-dev}
mkdir -p "$CF_WORK"
# cf_linux <command...>: runs a command on Linux - here, or in the distro.
cf_linux() {
  if [ "$cf_win" = 1 ]; then
    MSYS_NO_PATHCONV=1 wsl.exe -d "$CF_DISTRO" -e "$@"
  else
    "$@"
  fi
}
# cf_lpath <path>: a path as Linux sees it.
cf_lpath() {
  if [ "$cf_win" = 1 ]; then
    (cd "$1" 2>/dev/null && pwd -W || echo "$1") | sed 's#^\([A-Za-z]\):#/mnt/\L\1#'
  else
    echo "$1"
  fi
}
