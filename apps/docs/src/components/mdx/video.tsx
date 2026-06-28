import type { CSSProperties } from "react";

import { basePath } from "@/lib/base-path";

// Self-hosted video for MDX content. Files live under `public/videos`.
//
// Raw `<video src>` does not get the GitHub Pages `basePath` applied the way
// Next's `<Link>`/`<Image>` do, so we prefix it here from the single source of
// truth. This renders server-side during static export, where `basePath`
// resolves correctly (see src/lib/base-path.ts).
export function Video({
  src,
  style,
  ...props
}: {
  src: string;
  style?: CSSProperties;
} & React.VideoHTMLAttributes<HTMLVideoElement>) {
  return (
    <video
      src={`${basePath}${src}`}
      controls
      playsInline
      muted
      loop
      style={{ width: "100%", borderRadius: 8, ...style }}
      {...props}
    />
  );
}
