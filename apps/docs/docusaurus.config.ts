import { themes as prismThemes } from "prism-react-renderer";
import type { Config } from "@docusaurus/types";
import type * as Preset from "@docusaurus/preset-classic";

const config: Config = {
  title: "React Native WebGPU",
  tagline: "WebGPU for React Native, powered by Dawn",
  favicon: "img/favicon.svg",

  url: "https://wcandillon.github.io",
  baseUrl: "/react-native-webgpu/",

  organizationName: "wcandillon",
  projectName: "react-native-webgpu",

  onBrokenLinks: "throw",
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: "throw",
    },
  },

  i18n: {
    defaultLocale: "en",
    locales: ["en"],
  },

  presets: [
    [
      "classic",
      {
        docs: {
          sidebarPath: "./sidebars.ts",
          routeBasePath: "docs",
          editUrl:
            "https://github.com/wcandillon/react-native-webgpu/edit/main/apps/docs/",
        },
        blog: false,
        theme: {
          customCss: "./src/css/custom.css",
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    colorMode: {
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: "React Native WebGPU",
      items: [
        {
          type: "docSidebar",
          sidebarId: "docs",
          position: "left",
          label: "Docs",
        },
        {
          href: "https://www.npmjs.com/package/react-native-wgpu",
          label: "npm",
          position: "right",
        },
        {
          href: "https://github.com/wcandillon/react-native-webgpu",
          label: "GitHub",
          position: "right",
        },
      ],
    },
    footer: {
      style: "dark",
      links: [
        {
          title: "Docs",
          items: [
            { label: "Getting Started", to: "/docs/intro" },
            { label: "Installation", to: "/docs/installation" },
            {
              label: "Differences with the Web",
              to: "/docs/differences/frame-scheduling",
            },
          ],
        },
        {
          title: "Integrations",
          items: [
            { label: "Three.js", to: "/docs/integrations/three-js" },
            { label: "TensorFlow.js", to: "/docs/integrations/tensorflow" },
            { label: "Vision Camera", to: "/docs/integrations/vision-camera" },
            { label: "TypeGPU", to: "/docs/integrations/typegpu" },
          ],
        },
        {
          title: "More",
          items: [
            {
              label: "GitHub",
              href: "https://github.com/wcandillon/react-native-webgpu",
            },
            {
              label: "npm",
              href: "https://www.npmjs.com/package/react-native-wgpu",
            },
          ],
        },
      ],
      copyright: "Made with ❤️ in beautiful Zürich, Switzerland",
    },
    prism: {
      theme: prismThemes.oneLight,
      darkTheme: prismThemes.vsDark,
      additionalLanguages: ["bash", "diff", "json", "wgsl"],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
