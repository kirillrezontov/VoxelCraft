# ADR 0010: Declarative packs and transactional Lua hooks

Status: accepted for v0.7 development.

The transferred repository contained a partly implemented mod runtime that was
not yet connected to play, saving or tests. Complete that CPU gameplay service
before adding more features.

Packs register blocks before saved palettes resolve, then items and recipes
before inventory restoration. Manifests declare API 1 and exact dependencies;
invalid packs fail startup and registry construction is transactional. Reuse
built-in atlas tiles for this slice instead of introducing an asset pipeline.
Added recipes use the same transactional inventory implementation and paged UI.

Lua 5.4.9 builds as C++ so protected errors unwind callback objects. A separate
state per pack has bounded allocation/instructions and selected standard
libraries. Hook effects on blocks, inventory, persistent state and messages stage
until success. Failure discards those effects, disables the script and reports a
diagnostic; other scripts continue. Hooks notify after player actions succeed,
and script edits do not recursively emit hooks. VM budgets do not constitute
wall-clock preemption or an OS sandbox; packs must be trusted local code.

Player payload v6 appends namespaced mod versions/state and reads v1..v5. Missing
packs retain records and existing missing block/item handling, so removing a pack
does not silently erase its content keys or state. Changed versions produce a
diagnostic; schema migrations and strict mod-set pinning remain future work.

External textures, harvest rule declarations, native plugins, generation hooks,
hot reload and a stable API promise are outside this foundation. Windows and
human playtesting require separate verification.
