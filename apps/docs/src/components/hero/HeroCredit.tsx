import type { ShaderEntry } from "./types";

interface HeroCreditProps {
  entry: ShaderEntry;
  // Whether the shader renders dark (light overlay text) or light (dark text).
  dark: boolean;
}

// Bottom-corner attribution overlay. `pointer-events-auto` so the source link is
// clickable even though the hero container is otherwise click-through.
export function HeroCredit({ entry, dark }: HeroCreditProps) {
  const base = dark ? "text-white/65" : "text-neutral-600";
  const strong = dark ? "text-white/90" : "text-neutral-900";

  const body = (
    <span className={`text-xs ${base}`}>
      <span className={strong}>{entry.title}</span>
      {" · "}
      {entry.author}
    </span>
  );

  return (
    <div className="pointer-events-auto absolute bottom-3 right-4 z-20 select-none">
      {entry.sourceUrl ? (
        <a
          href={entry.sourceUrl}
          target="_blank"
          rel="noreferrer noopener"
          className="transition-opacity hover:opacity-100 opacity-80"
          title={`Open source for "${entry.title}"`}
        >
          {body}
        </a>
      ) : (
        <span className="opacity-80">{body}</span>
      )}
    </div>
  );
}
