#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
release_root="$project_root/dist/Release"

for bundle in "$release_root"/*.component "$release_root"/*.vst3; do
  [[ -d "$bundle" ]] || continue
  print "== $bundle =="
  plutil -p "$bundle/Contents/Info.plist"
  while IFS= read -r binary; do
    if file -b "$binary" | grep -q 'Mach-O'; then
      print -r -- "-- $binary"
      lipo -archs "$binary"
      otool -arch x86_64 -l "$binary" | sed -n '/LC_BUILD_VERSION/,+4p'
      otool -arch arm64 -l "$binary" | sed -n '/LC_BUILD_VERSION/,+4p'
      otool -L "$binary"
    fi
  done < <(find "$bundle" -type f)
  codesign --verify --deep --strict --verbose=2 "$bundle"
done
