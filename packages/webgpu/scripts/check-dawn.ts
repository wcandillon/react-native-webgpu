#!/usr/bin/env tsx

// Dawn is pinned in binaries.json and published by react-native-skia.
// Headers are prepared from that release before building/publishing. This checks it
// against the C++ workarounds below, which exist for Dawn bugs that an upstream
// fix makes obsolete. Each entry names the Dawn commit it was validated against.
// Selecting a release built from another commit fails here
// until whoever does the bump checks whether the upstream fix is included: if
// it is, delete the marked code and the entry; if it is not, update `commit`
// (and re-verify the diagnostic screen).
//
// Runs from `yarn check-dawn`, in CI right after `yarn install`.

import { existsSync, readFileSync, readdirSync } from "fs";
import { join } from "path";

const root = join(__dirname, "..");
const binaries = JSON.parse(readFileSync(join(root, "binaries.json"), "utf-8"));
const installedCommit: string | undefined = binaries.dawn?.commit;
if (!installedCommit) {
  console.error("❌ binaries.json must record dawn.commit");
  process.exit(1);
}
if (!existsSync(join(root, "cpp/dawn/include/webgpu/webgpu.h"))) {
  console.error(
    "❌ Dawn headers are missing. Run yarn copy-dawn-headers before building or checking Dawn."
  );
  process.exit(1);
}

const dawnWorkarounds = [
  {
    marker: "DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE",
    commit: "1716e88cc9e58398fab5d4473f7f2e858667b677",
    files: ["cpp/rnwgpu/SurfaceRegistry.h", "cpp/rnwgpu/api/GPUDevice.cpp"],
    // Android: device.destroy() before the native view is dropped crashed in
    // SwapChain::DetachFromSurfaceImpl (Vulkan FencedDeleter of the dead
    // device). Verify with the "Device Destroy Before Detach" diagnostic in
    // the example app.
    upstream: "Dawn CL pending (see the workaround comment in the files)",
  },
  {
    // Not a workaround: an internal Dawn header absent from the public headers.
    // The functions it declares are exported by the Dawn binaries; the header
    // has to match the Dawn the binaries were built from.
    marker: "DAWN_VENDORED_EXTERNAL_TEXTURE_PARAMS",
    commit: "1716e88cc9e58398fab5d4473f7f2e858667b677",
    files: ["cpp/dawn/common/ExternalTextureParams.h"],
    upstream:
      "diff the copy against src/dawn/common/ExternalTextureParams.h at the new commit and re-sync it",
  },
];

// Until #507 the build copied Dawn's headers into cpp/webgpu and cpp/dawn.
// Both can shadow the prepared include directory. cpp/dawn/common is vendored
// on purpose; cpp/dawn/include holds the current release headers.
const staleHeaderDirs = [
  "cpp/webgpu",
  ...(existsSync(join(root, "cpp/dawn"))
    ? readdirSync(join(root, "cpp/dawn"))
        .filter(
          (entry) =>
            entry !== "common" && entry !== "include" && !entry.startsWith(".")
        )
        .map((entry) => `cpp/dawn/${entry}`)
    : []),
].filter((dir) => existsSync(join(root, dir)));
if (staleHeaderDirs.length > 0) {
  console.error(
    `❌ Stale Dawn header copies shadow cpp/dawn/include: ${staleHeaderDirs.join(
      ", "
    )}. Delete them and run yarn copy-dawn-headers.`
  );
  process.exit(1);
}

let failed = false;
for (const workaround of dawnWorkarounds) {
  const present = workaround.files.filter((file) => {
    const path = join(root, file);
    return (
      existsSync(path) &&
      readFileSync(path, "utf-8").includes(workaround.marker)
    );
  });
  if (present.length === 0) {
    console.error(
      `❌ Workaround ${workaround.marker} is gone from the sources; remove its entry from scripts/check-dawn.ts`
    );
    failed = true;
  } else if (present.length !== workaround.files.length) {
    const missing = workaround.files.filter((file) => !present.includes(file));
    console.error(
      `❌ Workaround ${
        workaround.marker
      } is only partially applied: missing from ${missing.join(", ")}`
    );
    failed = true;
  } else if (installedCommit !== workaround.commit) {
    console.error(
      `❌ Dawn changed (${workaround.commit.slice(
        0,
        10
      )} -> ${installedCommit.slice(0, 10)}, binaries ${
        binaries.version
      }) but ${workaround.marker} is still in ${present.join(", ")}`
    );
    console.error(
      `   Check whether this Dawn contains the upstream fix (${workaround.upstream}). If it does, remove the marked code and this entry; if not, update the commit in scripts/check-dawn.ts`
    );
    failed = true;
  }
}
if (failed) {
  process.exit(1);
}

console.log(
  `✅ binaries ${binaries.version} (Dawn ${installedCommit.slice(
    0,
    10
  )}) at ${join(root, "cpp/dawn/include")}`
);
