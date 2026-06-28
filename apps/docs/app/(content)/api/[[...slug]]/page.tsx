import { createRelativeLink } from "fumadocs-ui/mdx";
import {
  DocsBody,
  DocsDescription,
  DocsPage,
  DocsTitle,
} from "fumadocs-ui/page";
import type { Metadata } from "next";
import { notFound } from "next/navigation";

import { getMDXComponents } from "@/components/mdx";
import { apiSource } from "@/lib/api-source";

export default async function Page(props: PageProps<"/api/[[...slug]]">) {
  const params = await props.params;
  const page = apiSource.getPage(params.slug);
  if (!page) {
    notFound();
  }

  const MDX = page.data.body;

  return (
    <DocsPage toc={page.data.toc} full={page.data.full}>
      <DocsTitle>{page.data.title}</DocsTitle>
      <DocsDescription>{page.data.description}</DocsDescription>
      <DocsBody>
        <MDX
          components={getMDXComponents({
            a: createRelativeLink(apiSource, page),
          })}
        />
      </DocsBody>
    </DocsPage>
  );
}

export async function generateStaticParams() {
  return [{ slug: [] }, ...apiSource.generateParams()];
}

export async function generateMetadata(
  props: PageProps<"/api/[[...slug]]">,
): Promise<Metadata> {
  const params = await props.params;
  const page = apiSource.getPage(params.slug);
  if (!page) {
    notFound();
  }

  return {
    title: `${page.data.title} | API`,
    description: page.data.description,
  };
}
