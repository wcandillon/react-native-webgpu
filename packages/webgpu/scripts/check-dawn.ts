#!/usr/bin/env tsx

// Dawn ships in the react-native-webgpu-dawn package, built by react-native-skia
// from the Dawn commit its Skia release pins. This checks the installed one
// against the C++ workarounds below, which exist for Dawn bugs that an upstream
// fix makes obsolete. Each entry names the Dawn commit it was validated against.
// Bumping the dependency to a package built from another commit fails here
// until whoever does the bump checks whether the upstream fix is included: if
// it is, delete the marked code and the entry; if it is not, update `commit`
// (and re-verify the diagnostic screen).
//
// Runs from `yarn check-dawn`, in CI right after `yarn install`.

import { existsSync, readFileSync } from "fs";
import { dirname, join } from "path";

const dawnPackageJson =
  require.resolve("react-native-webgpu-dawn/package.json");
const dawnPackage = JSON.parse(readFileSync(dawnPackageJson, "utf-8"));
const installedCommit: string | undefined = dawnPackage.dawn?.commit;
if (!installedCommit) {
  console.error(
    `❌ ${dawnPackageJson} does not record the Dawn commit it was built from (dawn.commit)`,
  );
  process.exit(1);
}

const dawnWorkarounds = [
  {
    marker: "DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE",
    commit: "3d786993a7ded64c4ebb4884b9b079db9ad0e580",
    files: ["cpp/rnwgpu/SurfaceRegistry.h", "cpp/rnwgpu/api/GPUDevice.cpp"],
    // Android: device.destroy() before the native view is dropped crashed in
    // SwapChain::DetachFromSurfaceImpl (Vulkan FencedDeleter of the dead
    // device). Verify with the "Device Destroy Before Detach" diagnostic in
    // the example app.
    upstream: "Dawn CL pending (see the workaround comment in the files)",
  },
];

const root = join(__dirname, "..");
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
      `❌ Workaround ${workaround.marker} is gone from the sources; remove its entry from scripts/check-dawn.ts`,
    );
    failed = true;
  } else if (present.length !== workaround.files.length) {
    const missing = workaround.files.filter((file) => !present.includes(file));
    console.error(
      `❌ Workaround ${workaround.marker} is only partially applied: missing from ${missing.join(", ")}`,
    );
    failed = true;
  } else if (installedCommit !== workaround.commit) {
    console.error(
      `❌ Dawn changed (${workaround.commit.slice(0, 10)} -> ${installedCommit.slice(0, 10)}, react-native-webgpu-dawn ${dawnPackage.version}) but ${workaround.marker} is still in ${present.join(", ")}`,
    );
    console.error(
      `   Check whether this Dawn contains the upstream fix (${workaround.upstream}). If it does, remove the marked code and this entry; if not, update the commit in scripts/check-dawn.ts`,
    );
    failed = true;
  }
}
if (failed) {
  process.exit(1);
}

console.log(
  `✅ react-native-webgpu-dawn ${dawnPackage.version} (Dawn ${installedCommit.slice(0, 10)}) at ${dirname(dawnPackageJson)}`,
);
