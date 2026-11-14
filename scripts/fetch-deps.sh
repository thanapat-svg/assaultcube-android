#!/usr/bin/env bash
# Fetch third-party libraries used by the Android build into android/app/jni/deps
set -euo pipefail
cd "$(dirname "$0")/../android/app/jni"
mkdir -p deps && cd deps

fetch() { # name repo ref
  if [ ! -d "$1" ]; then
    git clone --depth 1 --branch "$3" "https://github.com/$2.git" "$1"
  fi
}
fetch SDL         libsdl-org/SDL        release-2.30.x
fetch SDL_image   libsdl-org/SDL_image  release-2.8.x
fetch ogg         xiph/ogg              v1.3.5
fetch vorbis      xiph/vorbis           v1.3.7
fetch openal-soft kcat/openal-soft      1.23.1
fetch gl4es       ptitSeb/gl4es         master
