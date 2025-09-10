/* eslint-disable max-len */
/* eslint-disable camelcase */

import { chdir } from "process";

import type { Platform } from "./dawn-configuration";
import { $, mapKeys } from "./util";
import {
  build,
  checkBuildArtifacts,
  copyHeaders,
  copyLib,
  libs,
  projectRoot,
} from "./dawn-configuration";

const commonArgs = {
  CMAKE_BUILD_TYPE: "Release",
  BUILD_SAMPLES: "OFF",
  TINT_BUILD_TESTS: "OFF",
  TINT_BUILD_CMD_TOOLS: "OFF",
  TINT_BUILD_IR_BINARY: "OFF",
  DAWN_BUILD_SAMPLES: "OFF",
  DAWN_USE_GLFW: "OFF",
  DAWN_FETCH_DEPENDENCIES: "ON",
  DAWN_ENABLE_DESKTOP_GL: "OFF",
  BUILD_SHARED_LIBS: "OFF",
};

const PLATFORM_MAP: Record<string, string> = {
  arm64_iphoneos: "OS64",
  arm64_iphonesimulator: "SIMULATORARM64",
  x86_64_iphonesimulator: "SIMULATOR64",
  arm64_xros: "VISIONOS",
  arm64_xrsimulator: "SIMULATOR_VISIONOS",
  x86_64_xrsimulator: "SIMULATOR64_VISIONOS",
  universal_macosx: "MAC_UNIVERSAL",
};

const android = {
  platforms: ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"] as Platform[],
  args: {
    CMAKE_TOOLCHAIN_FILE: "$ANDROID_NDK/build/cmake/android.toolchain.cmake",
    ANDROID_PLATFORM: "android-26",
    DAWN_ENABLE_OPENGLES: "ON",
    DAWN_BUILD_MONOLITHIC_LIBRARY: "SHARED",
    CMAKE_EXE_LINKER_FLAGS: "-llog -Wl,-z,max-page-size=16384",
    CMAKE_SHARED_LINKER_FLAGS: "-llog -Wl,-z,max-page-size=16384",
    ...commonArgs,
  },
};

const apple = {
  matrix: {
    arm64: ["iphoneos", "iphonesimulator", "xros", "xrsimulator"],
    x86_64: ["iphonesimulator"],
    universal: ["macosx"],
  },
  args: {
    CMAKE_TOOLCHAIN_FILE: `${__dirname}/apple.toolchain.cmake`,
    DAWN_ENABLE_OPENGLES: "OFF",
    DAWN_BUILD_MONOLITHIC_LIBRARY: "STATIC",
    //  -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=13.0 -DENABLE_BITCODE=OFF -DENABLE_ARC=OFF -DENABLE_VISIBILITY=OFF
    ...commonArgs,
  },
};

(async () => {
  process.chdir("../..");

  // Build Android
  for (const platform of android.platforms) {
    console.log(`Build Android: ${platform}`);
    await build(
      `android_${platform}`,
      {
        ANDROID_ABI: platform,
        ...android.args,
      },
      `🤖 ${platform}`,
    );
    copyLib("android", platform);
  }

  // Build Apple
  for (const platform of mapKeys(apple.matrix)) {
    console.log(`Build Apple: ${platform}`);
    for (const sdk of apple.matrix[platform]) {
      await build(
        `apple_${platform}_${sdk}`,
        {
          PLATFORM: PLATFORM_MAP[`${platform}_${sdk}`],
          ...apple.args,
        },
        `🍏 ${platform} ${sdk}`,
      );
      copyLib("apple", platform, sdk);
    }
  }

  $(`mkdir -p ${projectRoot}/libs/apple/iphonesimulator`);
  libs.forEach((lib) => {
    console.log(`📱 Building fat binary for iphone simulator: ${lib}`);
    $(
      `lipo -create ${projectRoot}/libs/apple/x86_64_iphonesimulator/${lib}.a ${projectRoot}/libs/apple/arm64_iphonesimulator/${lib}.a -output ${projectRoot}/libs/apple/iphonesimulator/${lib}.a`,
    );
  });

  libs.forEach((lib) => {
    console.log(`📱 Building ${lib} (XCFramework) for iOS, visionOS and macOS`);

    $(`rm -rf ${projectRoot}/libs/apple/${lib}.xcframework`);
    $(
      "xcodebuild -create-xcframework " +
        `-library ${projectRoot}/libs/apple/iphonesimulator/${lib}.a ` +
        `-library ${projectRoot}/libs/apple/arm64_iphoneos/${lib}.a ` +
        `-library ${projectRoot}/libs/apple/arm64_xros/${lib}.a ` +
        `-library ${projectRoot}/libs/apple/arm64_xrsimulator/${lib}.a ` +
        `-library ${projectRoot}/libs/apple/universal_macosx/${lib}.a ` +
        ` -output ${projectRoot}/libs/apple/${lib}.xcframework `,
    );

    // Remove intermediate libraries
    $(
      "rm -rf " +
        `${projectRoot}/libs/apple/iphonesimulator` +
        `${projectRoot}/libs/apple/arm64_iphoneos` +
        `${projectRoot}/libs/apple/arm64_xros` +
        `${projectRoot}/libs/apple/arm64_xrsimulator` +
        `${projectRoot}/libs/apple/universal_macosx`,
    );
  });

  copyHeaders();
  chdir(projectRoot);
  checkBuildArtifacts();
})();
