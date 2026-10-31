# 0015: Application-owned frame loop and coroutine-owned sessions

Status: implemented for Milestone 2.

The previous local and remote functions each constructed a window/renderer, ran
nested loading/gameplay/save loops and returned only when the process was ready
to exit. Their stack locals already had valid RAII ownership, but extracting every
local into a large state struct would unnecessarily rewrite working algorithms.

Use one `ClientApplication` owning Window, renderer, settings and navigation, and
a move-only `GameSession` owning one standard C++ coroutine frame. The frame keeps
the existing local values, callbacks and destruction ordering. Yield at explicit
frame boundaries instead of polling SDL inside sessions. The host resumes once per
frame and handles result/error only after dispatch has returned. This is coarse
control flow, with no coroutine/virtual dispatch in voxel processing.

The frame starts suspended, preventing construction-side effects before publication.
Destroying it unwinds values and joins existing workers. Normal local leave must
observe a successful final snapshot before frame destruction; the destructor alone
is a cleanup fallback. Remote leave keeps server authority and closes transport.

Consequences: one frame allocation per session; explicit host-reference lifetime;
no synchronous waiting loops for save/generation/handshake; portable C++23 coroutine
support required from GCC, Clang and MSVC. GCC/Clang are exercised; MSVC remains a
platform verification requirement. Synchronous parsing/DNS are not made asynchronous.
No general coroutine scheduler, executor, service locator or new simulation is added.

The renderer persists and offers fence-protected session clearing/atlas replacement.
The legacy gameplay UI and a small transitional application menu remain until the
planned VoxelUI migration. See [implemented contracts and verification](../client-sessions.md).
