"use client";

/**
 * Renders nothing for the docs sidebar's `full` search trigger so the search
 * box does not appear in the sidebar. Defined in a client module because the
 * docs layout is a Server Component, and only client component references (not
 * inline functions) can be passed as slot props across that boundary.
 */
export function HiddenSidebarSearch() {
  return null;
}
