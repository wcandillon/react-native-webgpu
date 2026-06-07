import defaultComponents from "fumadocs-ui/mdx";
import { TypeTable } from "fumadocs-ui/components/type-table";
import type { MDXComponents } from "mdx/types";

import { Mermaid } from "@/components/mdx/mermaid";
import { WebGPUPlayground } from "@/components/playground/WebGPUPlayground";

export function getMDXComponents(components?: MDXComponents): MDXComponents {
  return {
    ...defaultComponents,
    TypeTable,
    Mermaid,
    WebGPUPlayground,
    ...components,
  };
}
