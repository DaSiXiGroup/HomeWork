# Style pass: shorter names

This pass keeps the latest project structure and shortens overlong identifiers.

Main rules:

- Keep public behavior unchanged.
- Use short but readable names: `AlgKind`, `SearchRes`, `SearchFrm`, `BenchRow`, `AgentRow`.
- Keep the double-underscore private style requested earlier.
- Shorten private UI state names, for example `__status`, `__frmIdx`, `__animSpd`, `__selCell`, `__camDist`.
- Shorten palette names, for example `Pal::AccentD`, `Pal::Path2CoreL`, `Pal::DioRoadD`.
- Avoid absurd one-letter names except for local geometric variables where `x/y/r/c` are already natural.

Checked with C++17 syntax-only build using a local raylib stub. Real raylib linking was not run here.
