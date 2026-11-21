#!/usr/bin/env bash
# Bygger SA-670 (VST3 + AU + Standalone) som universal binary och installerar
# till ~/Library/Audio/Plug-Ins/{VST3,Components}.
set -euo pipefail
cd "$(dirname "$0")"
cmake -B build -G Xcode
cmake --build build --config Release -- -quiet
echo
echo "Klart. VST3: ~/Library/Audio/Plug-Ins/VST3/SA-670.vst3"
echo "       AU:   ~/Library/Audio/Plug-Ins/Components/SA-670.component"
