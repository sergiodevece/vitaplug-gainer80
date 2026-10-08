#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
build_root="$project_root/build"
product_name="VitaPlug Gainer80"
release_root="$project_root/dist/Release"

build_slice() {
  local architecture="$1"
  local deployment_target="$2"
  local build_dir="$build_root/$architecture"

  cmake -S "$project_root" -B "$build_dir" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$architecture" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target"
  cmake --build "$build_dir" --config Release --parallel
  ctest --test-dir "$build_dir" --output-on-failure
}

find_bundle() {
  local root="$1"
  local suffix="$2"
  local result
  result=$(find "$root" -type d -name "*.$suffix" -print -quit)
  [[ -n "$result" ]] || { print -u2 "No .$suffix bundle found below $root"; exit 1; }
  print -r -- "$result"
}

merge_bundle() {
  local suffix="$1"
  local intel_bundle arm_bundle output_bundle relative intel_file arm_file
  intel_bundle=$(find_bundle "$build_root/x86_64" "$suffix")
  arm_bundle=$(find_bundle "$build_root/arm64" "$suffix")
  output_bundle="$release_root/${product_name}.$suffix"

  [[ "$(plutil -extract CFBundleIdentifier raw "$intel_bundle/Contents/Info.plist")" == "$(plutil -extract CFBundleIdentifier raw "$arm_bundle/Contents/Info.plist")" ]]
  [[ "$(plutil -extract CFBundleShortVersionString raw "$intel_bundle/Contents/Info.plist")" == "$(plutil -extract CFBundleShortVersionString raw "$arm_bundle/Contents/Info.plist")" ]]
  cmp -s "$intel_bundle/Contents/Info.plist" "$arm_bundle/Contents/Info.plist"
  if [[ -d "$intel_bundle/Contents/Resources" || -d "$arm_bundle/Contents/Resources" ]]; then
    [[ -d "$intel_bundle/Contents/Resources" && -d "$arm_bundle/Contents/Resources" ]]
    diff -ru --exclude _CodeSignature "$intel_bundle/Contents/Resources" "$arm_bundle/Contents/Resources"
  fi

  rm -rf "$output_bundle"
  ditto "$arm_bundle" "$output_bundle"

  while IFS= read -r arm_file; do
    if file -b "$arm_file" | grep -q 'Mach-O'; then
      relative="${arm_file#$arm_bundle/}"
      intel_file="$intel_bundle/$relative"
      [[ -f "$intel_file" ]] || { print -u2 "Missing Intel Mach-O peer: $relative"; exit 1; }
      lipo -create "$intel_file" "$arm_file" -output "$output_bundle/$relative"
    fi
  done < <(find "$arm_bundle" -type f)

  codesign --force --sign - --timestamp=none "$output_bundle"
  codesign --verify --deep --strict --verbose=2 "$output_bundle"
}

rm -rf "$release_root"
mkdir -p "$release_root"
build_slice x86_64 10.15
build_slice arm64 11.0
merge_bundle component
merge_bundle vst3

print "Universal artifacts:"
print "  $release_root/${product_name}.component"
print "  $release_root/${product_name}.vst3"
