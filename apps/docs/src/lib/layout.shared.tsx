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

export function homeLinks() {
  const basePath =
    process.env.NEXT_PUBLIC_BASE_PATH ??
    (process.env.GITHUB_PAGES === "true" &&
    process.env.NODE_ENV === "production"
      ? "/react-native-webgpu"
      : "");
  return {
    docs: `${basePath}/docs`,
    gettingStarted: `${basePath}/docs`,
    api: `${basePath}/api`,
  };
}
