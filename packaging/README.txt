VoxelCraft 0.8.2 release candidate - multiplayer preview

Extract the complete archive to a writable directory. A C++ compiler and Vulkan
SDK are not needed to play. The client needs a Vulkan 1.3 driver and an X11 or
Wayland desktop on Linux. Linux requires the glibc baseline of the build host
(official CI configuration: Ubuntu 24.04); libstdc++ and libgcc are bundled.
Windows archives include detected redistributable DLLs. Keep lib/ and assets/.
BUILD-INFO.txt records the compiler, configuration and runtime build baseline.

Single-player: ./voxelcraft                 Windows: voxelcraft.exe
Temporary play: ./voxelcraft --no-save
Select a world: ./voxelcraft --world-dir /path/to/world
Settings: --width 1280 --height 720 --windowed --vsync --radius 3
          --fov 75 --sensitivity 0.0025
F10 opens sensitivity/FOV settings; per-user settings and default offline worlds
use SDL's platform user-data directory. Base textures/content are built into the
executable. Shaders are under assets/shaders. Audio is not implemented.

Dedicated server: ./voxelcraft-server --world-dir worlds/friends --bind 0.0.0.0
                 --auth-file /private/server-identities.txt
Connect: ./voxelcraft --connect HOST:25565 --name Alice
         --identity-file /private/alice.token
Windows uses the same arguments with voxelcraft-server.exe / voxelcraft.exe.
Default server bind is loopback. Unauthenticated LAN hosting requires explicit
--trusted-lan. Use only trusted LANs or secured private tunnels: TCP is not
encrypted, and credentials must not cross an untrusted network.

Each line in server-identities.txt is NAME SPACE TOKEN. Tokens are unique random
256-bit values written as 64 lowercase hex digits. alice.token contains Alice's
token alone. Generate locally using Python: import secrets; print(secrets.token_hex(32)).
Share each token privately with its owner; restrict file permissions. The server
maps case-insensitive names to tokens; credentials are independent of saved worlds.
Existing names keep their inventories. Back up the credential file separately.

WASD/mouse: move/look; Space: jump; F: creative flight; 1-9/wheel: hotbar;
left/right: mine/place; E: inventory; C: survival crafting; R: respawn; F5: save;
Escape: release mouse/close menu. Close window to save and exit; server Ctrl-C
saves and stops. Failed saves retain live edits and report an error for retry.

World format 1 and player versions 1..10 remain supported; Lua/JSON mod API 1 is
unchanged. Network protocol 4 requires matching 0.8.2 clients and servers.
Optional demo pack: --mods-dir examples/mods on both client and server.

See docs/multiplayer.md, docs/world-format.md, docs/v0.8.2-release-notes.md,
and docs/v0.8.2-implementation.md for deployment limits and verification status.
Dependency license notices are under licenses/. This is a release candidate;
Windows runtime and cross-platform human LAN checks must be recorded before stable.

UI: E inventory, C crafting, F10 settings, Escape pause, / commands, T chat,
grave expanded console. See docs/v0.8.2-commands.md and v0.8.2-ui-design.md.
Fonts are embedded and also provided in assets/fonts with their license.
