#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tests_dir="$(cd "$script_dir/.." && pwd)"
image_dir="$tests_dir/images/compositor"

mkdir -p "$image_dir"

download() {
    local url="$1"
    local target="$2"
    if [[ ! -f "$target" ]]; then
        curl -fL "$url" -o "$target"
    fi
}

download "https://raw.githubusercontent.com/opencv/opencv/master/samples/data/box.png" "$image_dir/box.png"
download "https://raw.githubusercontent.com/opencv/opencv/master/samples/data/box_in_scene.png" "$image_dir/box_in_scene.png"

printf '%s\n' "$image_dir/box.png" "$image_dir/box_in_scene.png"