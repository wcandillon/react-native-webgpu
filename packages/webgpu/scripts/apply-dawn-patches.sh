#!/usr/bin/env bash
# Applies the patches in scripts/dawn-patches to the Dawn checkout before
# it is built: locally from `yarn build-dawn` (scripts/build/dawn.ts) and in CI
# from .github/workflows/build-dawn.yml. Each patch is an upstream change that
# the pinned Dawn commit does not carry yet; drop a patch once a Dawn bump
# includes it (this script then reports it as already present).
#
# Usage: apply-dawn-patches.sh [dawn-checkout]   (default: externals/dawn)
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
patches_dir="$script_dir/dawn-patches"
dawn_dir="${1:-$script_dir/../../../externals/dawn}"

if [ ! -d "$dawn_dir/src/tint" ]; then
  echo "❌ Dawn checkout not found at $dawn_dir (is the externals/dawn submodule checked out?)" >&2
  exit 1
fi

for patch in "$patches_dir"/*.patch; do
  name="$(basename "$patch")"
  if git -C "$dawn_dir" apply --check "$patch" 2>/dev/null; then
    git -C "$dawn_dir" apply "$patch"
    echo "🩹 Applied $name"
  elif git -C "$dawn_dir" apply --reverse --check "$patch" 2>/dev/null; then
    echo "✅ $name is already in the Dawn checkout (applied earlier, or the pinned Dawn carries it and the patch can be dropped)"
  else
    echo "❌ $name does not apply to $dawn_dir:" >&2
    git -C "$dawn_dir" apply --check "$patch" >&2 || true
    echo "   The patched code changed upstream. Check whether the pinned Dawn already carries an equivalent change and drop or refresh the patch." >&2
    exit 1
  fi
done
