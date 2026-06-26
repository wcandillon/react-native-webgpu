// Single source of truth for the GitHub Pages base path.
//
// On the live Pages deploy we serve from a project subpath
// (wcandillon.github.io/react-native-webgpu), so `next.config.ts` sets this as
// the Next `basePath`. Everything that needs the prefix should read it from
// here rather than re-deriving it, so the condition stays in one place.
//
// Note: this resolves to "" in any client bundle, because `GITHUB_PAGES` is not
// a `NEXT_PUBLIC_*` variable and is therefore not inlined on the client. Do not
// use it to build hrefs in client components — let Next's `<Link>` apply the
// base path automatically.
export const basePath =
  process.env.GITHUB_PAGES === "true" && process.env.NODE_ENV === "production"
    ? "/react-native-webgpu"
    : "";
