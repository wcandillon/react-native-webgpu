import type { HomeLayoutProps } from "fumadocs-ui/layouts/home";
import type { BaseLayoutProps } from "fumadocs-ui/layouts/shared";

export const docsShellClassName = "rnwgpu-docs-shell flex flex-col";
export const docsLayoutContainerClassName = "rnwgpu-docs-layout";
export const apiReferenceSidebarWidthClassName =
  "md:[--fd-sidebar-width:19rem] lg:[--fd-sidebar-width:20rem]";

const githubUrl = "https://github.com/wcandillon/react-native-webgpu";

export function sideBarOptions(): BaseLayoutProps {
  return {
    themeSwitch: {
      enabled: false,
    },
    searchToggle: {
      enabled: false,
    },
    nav: {
      title: "React Native WebGPU",
      url: "/",
    },
  };
}

export function docsLayoutSidebarOptions() {
  return {
    collapsible: false,
  } as const;
}

export function homeOptions(): HomeLayoutProps {
  return {
    links: [
      {
        text: "Docs",
        url: "/docs",
        active: "nested-url",
      },
      {
        text: "API Reference",
        url: "/api",
        active: "nested-url",
      },
    ],
    githubUrl,
    nav: {
      title: "React Native WebGPU",
      url: "/",
    },
  };
}

// Plain in-app paths. Do not prefix the GitHub Pages base path here: these are
// consumed by Next's `<Link>` (see app/(home)/page.tsx), which applies
// `basePath` from next.config automatically. Prefixing it manually would
// double it (e.g. /react-native-webgpu/react-native-webgpu/docs) on the
// Pages deploy.
export function homeLinks() {
  return {
    docs: "/docs",
    gettingStarted: "/docs",
    api: "/api",
  };
}
