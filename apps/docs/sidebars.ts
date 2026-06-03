import type { SidebarsConfig } from "@docusaurus/plugin-content-docs";

const sidebars: SidebarsConfig = {
  docs: [
    {
      type: "category",
      label: "Getting Started",
      collapsed: false,
      items: ["intro", "installation", "usage"],
    },
    {
      type: "category",
      label: "Differences with the Web",
      collapsed: false,
      items: [
        "differences/frame-scheduling",
        "differences/canvas-transparency",
        "differences/external-textures",
        "differences/shared-texture-memory",
        "differences/reanimated",
      ],
    },
    {
      type: "category",
      label: "Integrations",
      collapsed: false,
      items: [
        "integrations/three-js",
        "integrations/react-three-fiber",
        "integrations/tensorflow",
        "integrations/vision-camera",
        "integrations/typegpu",
      ],
    },
  ],
};

export default sidebars;
