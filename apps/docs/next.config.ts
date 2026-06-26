import { createMDX } from "fumadocs-mdx/next";
import type { NextConfig } from "next";

import { basePath } from "./src/lib/base-path";

const webgpuEntry = "./src/lib/react-native-webgpu.ts";

const withMDX = createMDX();

const config: NextConfig = {
  output: "export",
  basePath,
  assetPrefix: basePath ? `${basePath}/` : undefined,
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
