import { createMDX } from "fumadocs-mdx/next";
import type { NextConfig } from "next";

import { basePath } from "./src/lib/base-path";

const webgpuEntry = "./src/lib/react-native-webgpu.ts";

const withMDX = createMDX();

const config: NextConfig = {
  output: "export",
  basePath,
  assetPrefix: basePath ? `${basePath}/` : undefined,
  // Expose the base path to the client bundle. `basePath` itself is derived
  // from GITHUB_PAGES, which is not inlined client-side, so the static search
  // client needs this NEXT_PUBLIC_* mirror to build the correct index URL on
  // the Pages subpath deploy.
  env: {
    NEXT_PUBLIC_BASE_PATH: basePath,
  },
  images: { unoptimized: true },
  transpilePackages: [
    "react-native-webgpu",
    "react-native-web",
    "three",
    "@tensorflow/tfjs",
    "@tensorflow/tfjs-backend-webgpu",
  ],
  turbopack: {
    resolveAlias: {
      "react-native": "react-native-web",
      "react-native-webgpu": webgpuEntry,
    },
    resolveExtensions: [
      ".web.tsx",
      ".web.ts",
      ".web.js",
      ".tsx",
      ".ts",
      ".js",
    ],
  },
  webpack: (webpackConfig) => {
    webpackConfig.resolve ??= {};
    webpackConfig.resolve.alias = {
      ...webpackConfig.resolve.alias,
      "react-native$": "react-native-web",
      "react-native-webgpu": webgpuEntry,
    };
    webpackConfig.resolve.extensions = [
      ".web.tsx",
      ".web.ts",
      ".web.js",
      ...(webpackConfig.resolve.extensions ?? []),
    ];
    return webpackConfig;
  },
};

export default withMDX(config);
