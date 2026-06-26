import defaultComponents from "fumadocs-ui/mdx";
import { TypeTable } from "fumadocs-ui/components/type-table";
import * as Twoslash from "fumadocs-twoslash/ui";
import type { MDXComponents } from "mdx/types";

import { Mermaid } from "@/components/mdx/mermaid";
import { Video } from "@/components/mdx/video";
import { WebGPUPlayground } from "@/components/playground/WebGPUPlayground";

export function getMDXComponents(components?: MDXComponents): MDXComponents {
  return {
    ...defaultComponents,
    ...Twoslash,
    TypeTable,
    Mermaid,
    Video,
    WebGPUPlayground,
    ...components,
  };
}
