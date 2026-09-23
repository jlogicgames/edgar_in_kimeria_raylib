# Third-party sources

This directory contains unmodified, pinned upstream sources:

- Flecs v4.1.1 (`flecs/`), built with the project's restricted custom build flags.
- yxml commit `d41923630fcf70c6e2181722d9d087dd1aa3b530` (`yxml/`), a compact C89 XML
  parser for the Tiled readers added in Phase 2.

Their headers are consumed as system includes and their compilation intentionally does not
inherit first-party `-Werror` flags. See section 1.2 of `docs/migration-plan.md`.
