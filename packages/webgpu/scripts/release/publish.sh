#!/usr/bin/env bash
#
# Invoked by @semantic-release/exec (publishCmd) with the resolved version and
# dist-tag channel. semantic-release-yarn has already bumped
# packages/webgpu/package.json to $VERSION and packed the tarball into dist/.
#
# Publishes react-native-webgpu and its react-native-wgpu shim to npm in
# lockstep, both with provenance via the workflow's OIDC token (no NPM token).
set -euo pipefail

VERSION="$1"
CHANNEL="${2:-latest}"

PKG_DIR="$(cd "$(dirname "$0")/../.." && pwd)"      # packages/webgpu
SHIM_DIR="$(cd "$PKG_DIR/../webgpu-shim" && pwd)"   # packages/webgpu-shim
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT

# 1. react-native-webgpu: publish the tarball semantic-release-yarn produced.
#    Copy into a clean staging dir so only the intended tarball is present.
echo "Publishing react-native-webgpu@$VERSION (tag: $CHANNEL)"
cp "$PKG_DIR"/dist/*.tgz "$STAGING/"
( cd "$STAGING" && npm publish ./*.tgz --provenance --access public --tag "$CHANNEL" )
rm -f "$STAGING"/*.tgz

# 2. react-native-wgpu shim: same version, pinned to the matching release.
#    lib/ was built by `yarn build` earlier in the release workflow.
echo "Publishing react-native-wgpu@$VERSION (tag: $CHANNEL)"
( cd "$SHIM_DIR" && npm pkg set version="$VERSION" dependencies.react-native-webgpu="$VERSION" )
( cd "$SHIM_DIR" && yarn pack --out "$STAGING/react-native-wgpu.tgz" )
( cd "$STAGING" && npm publish ./react-native-wgpu.tgz --provenance --access public --tag "$CHANNEL" )
