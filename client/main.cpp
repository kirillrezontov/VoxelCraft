#include "client/options.hpp"
#include "client/preferences.hpp"
#include "client/network_client.hpp"
#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/generation.hpp"
#include "engine/voxel/terrain_worker.hpp"
#include "engine/voxel/storage.hpp"
#include "engine/voxel/snapshot_writer.hpp"
#include "engine/voxel/streaming.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/mesh_worker.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include "game/content/creature_models.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/player/spawn.hpp"
#include "game/gameplay/survival.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/gameplay/inventory.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "game/settings.hpp"
#include "game/simulation/day_cycle.hpp"
#include "game/simulation/creatures.hpp"
#include "game/modding/runtime.hpp"
#include <array>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

int run(const vc::client::Options& options) {
    vc::Window window(options.width,options.height,options.fullscreen);
    const auto preferences=vc::client::loadPreferences(options);
    const auto settings=preferences.settings;const auto settingsFile=preferences.path;
    const double generationStart = vc::Window::seconds();
    auto registry = vc::builtinBlocks();
    // Register content before WorldStore resolves saved names into runtime IDs.
    auto modsDirectory=options.modsDirectory;
    if(modsDirectory.empty()&&!options.noMods&&!options.smoke&&!options.frameLimit)
        modsDirectory=vc::Window::userDataPath()/"mods";
    vc::ModRuntime mods(modsDirectory,registry);
    const auto atlas=mods.atlas().layout();
    vc::VulkanRenderer renderer(window,mods.atlas().image(),options.requireValidation,options.vsync);
    vc::log("Loaded "+std::to_string(mods.size())+" mods");
    for(const auto& id:mods.order()) vc::log("Mod: "+id);
    vc::voxel::WorldMetadata metadata{options.seed,options.bounds,options.generatorVersion.value_or(vc::voxel::terrainGenerationVersion)};
    vc::voxel::World world;
    std::unique_ptr<vc::voxel::WorldStore> store;
    std::vector<vc::voxel::Position> savedChunks;
    std::optional<vc::PlayerSaveState> savedPlayer;
    // Bounded test/screenshot runs stay temporary unless given an explicit world.
    if(!options.noSave && !options.smoke && (!options.frameLimit || !options.worldDirectory.empty())) {
        const auto directory=options.worldDirectory.empty()?vc::Window::userDataPath()/"worlds"/"sandbox":options.worldDirectory;
        store=std::make_unique<vc::voxel::WorldStore>(directory);
        if(auto archive=store->load(registry)) {
            if(options.seedSpecified&&options.seed!=archive->metadata.seed)
                throw std::invalid_argument("--seed differs from the saved world; use a new --world-dir");
            if(options.generatorVersion&&*options.generatorVersion!=archive->metadata.generatorVersion)
                throw std::invalid_argument("Generator version differs from the saved world; use a new --world-dir");
            if((options.minSpecified&&options.bounds.minY!=archive->metadata.bounds.minY)||
                (options.maxSpecified&&options.bounds.maxY!=archive->metadata.bounds.maxY))
                throw std::invalid_argument("Height options differ from the saved world; use a new --world-dir");
            metadata=archive->metadata;world=std::move(archive->world);
            for(const auto& [p,chunk]:world.chunks()) { (void)chunk;savedChunks.push_back(p); }
            if(!archive->gameData.empty()) savedPlayer=vc::decodePlayer(archive->gameData);
            vc::log("Loaded world revision "+std::to_string(archive->revision)+" from "+vc::pathText(directory));
        }
    }
    if(metadata.bounds.minY < -1048576 || metadata.bounds.maxY > 1048575)
        throw std::invalid_argument("Saved world height exceeds the client's supported -1048576..1048575 range");
    if(!savedPlayer && world.chunks().empty()) world=vc::voxel::World(metadata.bounds);
    if(savedPlayer) mods.restore(savedPlayer->mods);
    const vc::voxel::TerrainGenerator generator(metadata.seed, metadata.bounds, registry,metadata.generatorVersion);
    bool survivalMode=options.survival.value_or(savedPlayer&&savedPlayer->survival);
    vc::log(std::string("Mode: ")+(survivalMode?"survival":"creative")+"; generator version "+std::to_string(metadata.generatorVersion));
    if(survivalMode&&options.fly) throw std::invalid_argument("Flight is disabled in survival; use --creative to change mode");
    vc::PlayerController player;
    auto& camera=player.camera;
    player.flying=options.fly;camera.yaw=0;camera.pitch=-0.55;
    if(!savedPlayer&&!options.fly) camera.position=vc::findSpawn(generator,registry,&world);
    if(options.fly) {
        const double distance=std::min(80.0,options.radius*24.0);
        camera.position={distance,static_cast<double>(generator.surfaceY(static_cast<std::int64_t>(distance),static_cast<std::int64_t>(distance)))+38,distance};
        camera.yaw=-0.65;camera.pitch=-0.38;
    }
    if(savedPlayer) { camera=savedPlayer->camera;player.flying=!survivalMode&&(options.fly||savedPlayer->flying); }
    const auto cameraBlock=[&] {
        return vc::voxel::Position{static_cast<std::int64_t>(std::floor(camera.position.x)),
            static_cast<std::int64_t>(std::floor(camera.position.y)),static_cast<std::int64_t>(std::floor(camera.position.z))};
    };
    std::vector<vc::voxel::Position> requested;
    for(const auto p:vc::voxel::streamingRegion(cameraBlock(),metadata.bounds,options.radius,metadata.generatorVersion))
        if(!world.findChunk(p)) requested.push_back(p);
    {
        vc::voxel::TerrainWorker worker(generator);
        std::size_t next = 0, completed=0;
        while (completed < requested.size()) {
            const auto input = window.poll();
            if (input.quit) return 0;
            if (input.resized) renderer.requestResize();
            while (next < requested.size() && worker.request(requested[next])) ++next;
            while (auto ready = worker.takeReady()) { world.insert(std::move(*ready));++completed; }
            window.title("VoxelCraft: generating terrain " + std::to_string(completed) + '/' + std::to_string(requested.size()));
            // Keep processing the desktop and presenting while CPU generation runs.
            renderer.draw(vc::Camera{});
        }
    }
    std::unique_ptr<vc::voxel::ChunkStreamer> streamer;
    if(!options.smoke) {
        streamer=std::make_unique<vc::voxel::ChunkStreamer>(world,generator,options.radius);
        streamer->retain(savedChunks);streamer->tick(cameraBlock());
    }
    const double meshingStart = vc::Window::seconds();
    vc::voxel::Lighting lighting;const auto initialDirty=world.takeDirtyChunks();
    (void)lighting.refresh(world,registry,initialDirty);
    std::unique_ptr<vc::voxel::BackgroundMeshQueue> backgroundMeshes;
    if(!options.smoke) backgroundMeshes=std::make_unique<vc::voxel::BackgroundMeshQueue>(registry,atlas);
    std::vector<vc::Mesh> meshes;
    std::optional<vc::voxel::Position> emptyCell;
    if(backgroundMeshes) {
        backgroundMeshes->invalidate(initialDirty);
        while(backgroundMeshes->pendingCount()) {
            const auto input=window.poll();if(input.quit) return 0;
            if(input.resized) renderer.requestResize();
            renderer.updateMeshes(backgroundMeshes->update(world,cameraBlock(),4,&lighting));
            window.title("VoxelCraft: preparing meshes "+std::to_string(backgroundMeshes->pendingCount())+" remaining");
            if(!renderer.draw(camera)) vc::Window::idle();
        }
    } else for (const auto& [position, chunk] : world.chunks()) {
        (void)position;
        auto mesh = vc::voxel::meshChunk(chunk,world,registry,atlas,&lighting);
        if (!mesh.indices.empty()||!mesh.translucentIndices.empty()) meshes.push_back(std::move(mesh));
        else if (!emptyCell) {
            const auto origin = vc::voxel::chunkOrigin(position);
            const vc::voxel::Position center{origin.x+16,origin.y+16,origin.z+16};
            if (world.bounds().contains(center.y)) emptyCell=center;
        }
    }
    const double uploadStart = vc::Window::seconds();
    if(!backgroundMeshes) renderer.upload(meshes);
    meshes.clear(); meshes.shrink_to_fit();
    (void)world.takeDirtyChunks();
    vc::voxel::ChunkMeshQueue meshQueue;
    vc::log("Seed " + std::to_string(metadata.seed) + "; loaded " + std::to_string(world.chunks().size()) +
        " chunks; drawable " + std::to_string(renderer.meshCount()) + "; triangles " + std::to_string(renderer.triangleCount()));
    vc::log("Generation " + std::to_string((meshingStart - generationStart) * 1000) + " ms; mesh preparation " +
        std::to_string((uploadStart - meshingStart) * 1000) + " ms; upload " + std::to_string((vc::Window::seconds() - uploadStart) * 1000) + " ms");
    vc::log("WASD: walk, Space: jump, F: toggle flight, left/right click: destroy/place, 1/2/3: grass/dirt/stone, Escape: release mouse");
    if(savedPlayer) {
        if(!player.flying&&vc::voxel::collides(world,registry,player.bounds())) {
            if(survivalMode) {
                const auto edits=streamer?streamer->snapshot():world;camera.position=vc::findSpawn(generator,registry,&edits);
                vc::log("Saved survival position was obstructed; moved to a clear spawn");
            } else { player.flying=true;vc::log("Saved player position is obstructed or unavailable; enabled flight for recovery"); }
        }
    }
    const std::array materials{registry.require("voxelcraft:grass"), registry.require("voxelcraft:dirt"), registry.require("voxelcraft:stone")};
    auto items=vc::builtinItems(registry);
    auto recipes=vc::builtinRecipes(items);mods.installItems(items,registry,recipes);
    if(savedPlayer) vc::retainHotbarItems(items,savedPlayer->hotbar);
    vc::CreativeInventory inventory(items);
    if(savedPlayer) inventory.restore(savedPlayer->hotbar,savedPlayer->selectedSlot,registry);
    vc::SurvivalState survival;
    auto spawnPoint=savedPlayer?savedPlayer->spawnPoint:std::optional<glm::dvec3>{};
    std::uint64_t poseRevision=0;
    const auto commandRegistry=vc::commands::builtinCommands();
    vc::LivingWorld living(metadata.seed);
    if(savedPlayer) { survival.inventory.restore(savedPlayer->items,savedPlayer->selectedSlot,items);survival.restoreHealth(savedPlayer->health); }
    if(savedPlayer) living.restore(savedPlayer->actors,items);
    renderer.setModels(vc::builtinCreatureModels());
    vc::MiningRules miningRules(registry,items);mods.installMining(miningRules,registry,items);
    vc::SurvivalUiContext survivalUi{&survival,&items,&recipes};
    vc::DayCycle day(options.timeOfDay?*options.timeOfDay*vc::DayCycle::length:(savedPlayer?savedPlayer->worldTime:300));
    auto material=inventory.selectedBlock().value_or(vc::voxel::air);
    vc::SandboxUiController ui(inventory,settings,survivalMode?&survivalUi:nullptr);
    ui.setCompletionContext({vc::commands::Permission::Administrator,&items,{"Player"}});
    const auto notify=[&](std::string message,vc::NotificationSeverity severity=vc::NotificationSeverity::Info) { ui.status(std::move(message),vc::Window::seconds(),severity); };
    const auto dispatchMod=[&](const vc::ModEvent& event) {
        for(auto& message:mods.dispatch(event,{world,registry,items,survivalMode?&survival:nullptr,player.bounds()})) {
            vc::log(message);notify(std::move(message));
        }
        for(const auto& diagnostic:mods.takeDiagnostics()) { vc::log(diagnostic);notify(diagnostic,vc::NotificationSeverity::Error); }
    };
    dispatchMod({vc::ModEventKind::Load});
    std::unique_ptr<vc::voxel::SnapshotWriter> snapshotWriter;
    if(store) snapshotWriter=std::make_unique<vc::voxel::SnapshotWriter>(*store);
    const auto finishSnapshot=[&](const vc::voxel::SnapshotResult& result) {
        result.rethrowIfFailed();
        vc::log("Saved world revision "+std::to_string(result.revision)+" in "+std::to_string(result.milliseconds)+" ms to "+vc::pathText(store->directory()));
        notify("SAVED WORLD "+std::to_string(result.revision),vc::NotificationSeverity::Success);
    };
    const auto awaitSnapshot=[&] {
        while(snapshotWriter->busy()) {
            if(const auto result=snapshotWriter->takeReady()) { finishSnapshot(*result);break; }
            const auto input=window.poll();if(input.resized) renderer.requestResize();
            window.title("VoxelCraft: saving world");if(!renderer.draw(camera)) vc::Window::idle();
        }
    };
    const auto save=[&](bool waitForCompletion=false) {
        if(!store) { notify("TEMPORARY WORLD - SAVING DISABLED");return; }
        if(snapshotWriter->busy()) {
            if(waitForCompletion) awaitSnapshot();
            else { notify("SAVE ALREADY IN PROGRESS");return; }
        }
        vc::PlayerSaveState state;state.camera=camera;state.flying=player.flying;
        state.hotbar=inventory.names(registry);state.selectedSlot=static_cast<std::uint32_t>(survivalMode?survival.inventory.selectedSlot():inventory.selectedSlot());
        state.survival=survivalMode;state.health=survival.health();state.items=survival.inventory.save(items);
        state.worldTime=day.seconds();state.spawnPoint=spawnPoint;
        state.actors=living.save(items);
        state.mods=mods.save();
        if(savedPlayer) state.profiles=savedPlayer->profiles;
        const auto gameData=vc::encodePlayer(state);
        if(!snapshotWriter->request(streamer?streamer->snapshot():world,registry,metadata,gameData)) throw std::logic_error("Snapshot request unexpectedly busy");
        notify("SAVING WORLD");if(waitForCompletion) awaitSnapshot();
    };
    const auto commandContext=[&](auto& hotbar,auto& selected) {
        vc::commands::Context context;context.permission=vc::commands::Permission::Administrator;context.issuer="Player";
        context.origin=camera.position-glm::dvec3(0,vc::PlayerController::eyeHeight,0);
        context.world=&world;context.blocks=&registry;context.items=&items;context.living=&living;context.day=&day;context.metadata=metadata;
        context.players.push_back({"Player",&player,&survival,&survivalMode,&hotbar,&selected,&spawnPoint,&poseRevision,context.permission});
        context.sample=[&](auto position) { return streamer?streamer->sample(position):world.loaded(position)?world.block(position):generator.sample(position); };
        context.save=[&] {
            if(!store) return vc::commands::Result::error(vc::commands::Code::Unavailable,"Saving is disabled in this temporary world");
            if(snapshotWriter->busy()) return vc::commands::Result::error(vc::commands::Code::Unavailable,"Save already in progress");
            save();return vc::commands::Result::ok("World save queued");
        };
        context.broadcast=[&](std::string_view message) { notify(std::string(message)); };
        return context;
    };
    bool smokeAimed = false, smokeDestroyed = false, smokePlaced = false, smokeJumped = false;
    std::optional<vc::voxel::Position> smokeEdit;
    vc::voxel::BlockId smokeOriginal{};
    unsigned editCount = 0;
    const auto initialMeshCount = renderer.meshCount();
    const double initialFootY = player.bounds().min.y;
    double highestFootY = initialFootY;
    bool smokeMeshAdded = false, smokeMeshRemoved = false;
    if (!options.smoke) { window.pointerMode(ui.open());window.capture(!ui.open()); }
    unsigned frames = 0, diagnosticFrames = 0;
    double last = vc::Window::seconds(), diagnosticStart = last, smokeStart = last, lastSave=last;
    double minimizedAt = 0;
    bool minimized = false, restored = false, firstResize = false, secondResize = false;
    bool released = false, recaptured = false, capturedScreenshot = false;
    bool savedOnExit=false,discardExit=false;
    auto screenshot = options.screenshot;
    if (options.smoke && screenshot.empty()) screenshot = window.assetPath().parent_path() / "smoke.ppm";
    bool consoleCaptureBefore=true;
    while (true) {
        const auto updateStart=vc::Window::seconds();
        auto input = window.poll();
        if(snapshotWriter) if(const auto result=snapshotWriter->takeReady()) {
            try { finishSnapshot(*result); }
            catch(const std::exception& error) { vc::log(std::string("Save failed; world remains in memory: ")+error.what());notify("SAVE FAILED - F5 RETRY - CTRL SHIFT Q QUIT"); }
        }
        if(input.quitWithoutSaving) { discardExit=true;break; }
        if(input.quit) {
            if(!options.frameLimit) {
                try { save(true);savedOnExit=true;break; }
                catch(const std::exception& error) {
                    vc::log(std::string("Close cancelled because saving failed; world remains in memory: ")+error.what());
                    notify("SAVE FAILED - F5 RETRY - CTRL SHIFT Q QUIT");window.capture(false);last=vc::Window::seconds();continue;
                }
            }
            break;
        }
        if (input.resized) renderer.requestResize();
        const double now = vc::Window::seconds();
        if(survivalMode&&!survival.alive()&&input.respawn) {
            try {
                const auto edits=streamer?streamer->snapshot():world;auto spawn=vc::findSpawn(generator,registry,&edits);
                auto names=inventory.names();auto selected=static_cast<std::uint32_t>(inventory.selectedSlot());
                if(spawnPoint&&vc::commands::safeFeet(commandContext(names,selected),*spawnPoint)) spawn=*spawnPoint+glm::dvec3(0,vc::PlayerController::eyeHeight,0);
                player=vc::PlayerController{};camera.position=spawn;camera.yaw=0;camera.pitch=-0.55;
                survival.respawn();ui.closeMenus();window.pointerMode(false);window.capture(true);input.captured=true;
                input.forward=input.right=input.up=input.mouseX=input.mouseY=0;input.destroy=input.place=input.destroyHeld=input.jump=false;
                notify("RESPAWNED - INVENTORY KEPT");
            } catch(const std::exception& error) { vc::log(error.what());notify("NO SAFE SPAWN - USE CREATIVE MODE"); }
        }
        const auto [w,h]=window.pixelSize();
        const bool closingConsole=ui.screen()==vc::UiScreen::Console&&!input.console;
        if(input.console) consoleCaptureBefore=input.consoleCapturedBefore;
        if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,
                input.material,input.scroll,input.cursorX,input.cursorY,input.settings,input.crafting,input.console,input.pause,input.primaryRelease,input.primaryHeld,input.shift,input.text},w,h)) {
            window.pointerMode(ui.open());window.capture(!ui.open()&&(!closingConsole||consoleCaptureBefore));
            input.forward=input.right=input.up=input.mouseX=input.mouseY=0;
            input.destroy=input.place=input.jump=input.toggleFlight=input.primaryClick=input.secondaryClick=false;
            input.captured=!ui.open()&&(!closingConsole||consoleCaptureBefore);
        }
        window.textInput(ui.wantsTextInput());window.pointerMode(ui.open());
        if(auto clipboard=ui.takeClipboard();!clipboard.empty()) window.clipboard(clipboard);
        for(const auto& command:ui.takeCommands()) {
            auto names=inventory.names();auto selected=static_cast<std::uint32_t>(inventory.selectedSlot());
            auto context=commandContext(names,selected);ui.postCommandResult(commandRegistry.execute(command,context),now);
            inventory.restore(names,selected);ui.setSurvivalContext(survivalMode?&survivalUi:nullptr);
        }
        if(ui.takeSaveRequested()) { try { save(); } catch(const std::exception& error) { ui.status(error.what(),now,vc::NotificationSeverity::Error); } }
        if(ui.open()) {
            input.forward=input.right=input.up=input.mouseX=input.mouseY=0;
            input.destroy=input.place=input.destroyHeld=input.jump=input.toggleFlight=false;input.captured=false;
        }
        for(auto& identifier:ui.takeCraftedItems()) dispatchMod({vc::ModEventKind::Craft,cameraBlock(),std::move(identifier)});
        camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);
        camera.mouseSensitivity=ui.settings().mouseSensitivity;camera.invertY=ui.settings().invertY;
        if(streamer) streamer->tick(cameraBlock());
        if(ui.takeSettingsChanged()) {
            if(settingsFile.empty()) notify("SETTINGS APPLY FOR THIS SESSION");
            else {
                try { vc::saveSettings(settingsFile,ui.settings());notify("SETTINGS SAVED"); }
                catch(const std::exception& error) { vc::log(std::string("Settings save failed: ")+error.what());notify("SETTINGS SAVE FAILED - SEE LOG"); }
            }
        }
        vc::PlayerInput playerInput{{input.forward, input.right, input.up, input.mouseX, input.mouseY}, input.jump, input.toggleFlight, input.captured};
        if(survivalMode) playerInput.toggleFlight=false;
        if (options.smoke) {
            if (now - smokeStart > 30) throw std::runtime_error("Graphics smoke test exceeded 30 seconds");
            if (frames >= 10 && !firstResize) { window.resize(960, 540); firstResize = true; renderer.requestResize(); }
            if (frames >= 25 && !secondResize) { window.resize(1280, 720); secondResize = true; renderer.requestResize(); }
            if (frames >= 35 && !released) { window.capture(false); released = true; }
            if (frames >= 40 && !recaptured) { window.capture(true); window.capture(false); recaptured = true; }
            if (frames >= 45 && !minimized) { window.minimize(); minimized = true; minimizedAt = now; }
            if (minimized && !restored && now - minimizedAt > 0.25) { window.restore(); renderer.requestResize(); restored = true; }
            // Drive the same player/editor path as live input. Place and remove at a
            // negative-coordinate chunk seam so neighboring GPU meshes must change.
            playerInput = {};
            if (frames >= 5 && !smokeJumped) { playerInput.jump = true; smokeJumped = true; }
            if (frames >= 60 && !smokeAimed) {
                if (!options.fly && (!player.grounded || highestFootY < initialFootY + 1.0))
                    throw std::runtime_error("Smoke walking/jump/landing check failed");
                player.flying = true;
                camera.position = {-0.5, static_cast<double>(generator.surfaceY(-1,0)) + 4.5, 0.5};
                camera.yaw = 0; camera.pitch = -1.55; smokeAimed = true;
            }
            if (frames >= 65 && !smokeDestroyed) input.destroy = true;
            if (frames >= 75 && smokeDestroyed && !smokePlaced) input.place = true;
        }
        const double delta=ui.paused()?0:options.smoke?1.0/60:now-last;
        if(!options.freezeTime) day.advance(std::clamp(delta,0.0,0.1));
        if(!ui.paused()&&(!survivalMode||survival.alive())) player.update(playerInput,delta,world,registry);
        if(survivalMode&&survival.alive()&&survival.land(player.landingSpeed)) {
            if(!survival.alive()) { ui.closeMenus();window.pointerMode(true);window.capture(false);notify("YOU DIED - R TO RESPAWN"); }
            else notify("FALL DAMAGE");
        }
        last=now;
        if(!ui.paused()) dispatchMod({vc::ModEventKind::Tick,{}, {},std::clamp(delta,0.0,0.1)});
        if (options.smoke && frames < 60) highestFootY = std::max(highestFootY, player.bounds().min.y);
        material=inventory.selectedBlock().value_or(vc::voxel::air);
        if(survivalMode) {
            const auto& stack=survival.inventory.selected();material=stack.empty()?vc::voxel::air:items.definition(stack.item).block.value_or(vc::voxel::air);
        }
        auto hit = vc::voxel::raycast(world,camera.position,camera.forward(),6,survivalMode?&registry:nullptr);
        const auto events=living.update(delta,world,registry,camera.position,survivalMode?&survival:nullptr,items,day.daylight().sunlight<0.12f,!options.smoke&&!ui.paused());
        if(events.collected) notify("COLLECTED LOOT");
        if(events.damage) {
            if(!survival.alive()) { ui.closeMenus();window.pointerMode(true);window.capture(false);notify("YOU DIED - R TO RESPAWN"); }
            else notify("CREATURE ATTACK");
        }
        const auto actor=living.target(world,registry,camera.position,camera.forward());
        if(actor&&input.destroyHeld&&input.captured&&!ui.open()&&(!survivalMode||survival.alive())) {
            const auto attack=living.attack(*actor,camera.position,survivalMode?&survival:nullptr,items);
            if(attack.hit) notify(attack.killed?"CREATURE DEFEATED - COLLECT LOOT":"HIT CREATURE");
            if(attack.noDropSpace) notify("LOOT LIMIT - COLLECT EXISTING DROPS");
        }
        bool changed = false;
        const auto brokenBlock=hit?registry.definition(world.block(hit->block)).identifier:std::string{};
        if(survivalMode) {
            const auto mined=survival.mine(input.destroyHeld&&input.captured&&!ui.open()&&!actor,hit,delta,world,items,miningRules);
            changed=mined.changed;
            if(mined.inventoryFull) notify("INVENTORY FULL - MINING STOPPED",vc::NotificationSeverity::Warning);
            if(mined.toolBroke) notify("TOOL BROKE",vc::NotificationSeverity::Warning);
        } else if (hit && input.destroy&&!ui.open()&&!actor) {
            if (options.smoke && !smokeDestroyed) { smokeEdit = hit->block; smokeOriginal = world.block(hit->block); }
            changed = vc::destroyBlock(world, *hit);
            if (options.smoke && changed) { smokeDestroyed = true; material = smokeOriginal; }
        }
        if(changed&&hit) dispatchMod({vc::ModEventKind::Break,hit->block,brokenBlock});
        const bool ate=survivalMode&&input.place&&input.captured&&!ui.open()&&survival.eat(items);
        if(ate) notify("ATE FOOD - HEALED 3");
        if (!ate&&!changed&&hit&&input.place&&!ui.open()&&(!survivalMode||(survival.alive()&&!input.destroyHeld))) {
            changed = vc::placeBlock(world, registry, *hit, material, player.bounds());
            if(changed&&survivalMode) (void)survival.inventory.consumeSelected();
            if(changed) dispatchMod({vc::ModEventKind::Place,*vc::voxel::offsetPosition(hit->block,hit->normal),registry.definition(material).identifier});
            if (options.smoke && changed) {
                if (!smokeEdit || world.block(*smokeEdit) != smokeOriginal) throw std::runtime_error("Smoke placement did not restore the destroyed cell");
                smokePlaced = true;
            }
        }
        bool lifecycleChange = false;
        if (options.smoke && emptyCell) {
            if (frames >= 85 && !smokeMeshAdded) {
                if (!world.setBlock(*emptyCell, materials[2])) throw std::runtime_error("Smoke temporary block creation failed");
                smokeMeshAdded = true; lifecycleChange = true;
            } else if (frames >= 95 && smokeMeshAdded && !smokeMeshRemoved) {
                if (!world.setBlock(*emptyCell, vc::voxel::air)) throw std::runtime_error("Smoke temporary block removal failed");
                smokeMeshRemoved = true; lifecycleChange = true;
            }
        }
        if(changed) ++editCount;
        const auto dirty=world.takeDirtyChunks();const auto lightDirty=lighting.refresh(world,registry,dirty);
        std::vector<vc::Mesh> replacements;
        if(backgroundMeshes) {
            backgroundMeshes->invalidate(dirty);backgroundMeshes->invalidate(lightDirty);
            replacements=backgroundMeshes->update(world,cameraBlock(),changed?4:2,&lighting);
        } else {
            meshQueue.invalidate(dirty);meshQueue.invalidate(lightDirty);
            replacements=meshQueue.rebuild(world,registry,atlas,cameraBlock(),128,&lighting);
        }
        renderer.updateMeshes(replacements);
        if (changed || lifecycleChange) {
            vc::log("World edit: updated " + std::to_string(replacements.size()) + " ready chunk meshes");
            if (lifecycleChange && renderer.meshCount() != initialMeshCount + (smokeMeshRemoved ? 0 : 1))
                throw std::runtime_error("Smoke GPU mesh add/remove count mismatch");
            hit = vc::voxel::raycast(world,camera.position,camera.forward(),6,survivalMode?&registry:nullptr);
        }
        if (hit&&!ui.open()&&!actor) renderer.setSelection(glm::i64vec3(hit->block.x, hit->block.y, hit->block.z));
        else renderer.setSelection(std::nullopt);
        renderer.setProgressIndicator(survivalMode&&!ui.open()&&survival.mining()?std::optional(static_cast<float>(survival.miningProgress())):std::nullopt);
        const auto [width,height]=window.pixelSize();
        const auto daylight=day.daylight();vc::Atmosphere atmosphere;
        atmosphere.skyColor=daylight.skyColor;atmosphere.sunDirection=daylight.sunDirection;atmosphere.sunlight=daylight.sunlight;atmosphere.ambient=daylight.ambient;
        atmosphere.horizontalFog=true;atmosphere.fogStart=static_cast<float>(std::max(8.0,(options.radius-0.8)*32));atmosphere.fogEnd=static_cast<float>(options.radius*32);
        if(registry.definition(world.block(cameraBlock())).fluid) {
            atmosphere.skyColor={0.008f,0.055f,0.12f};atmosphere.fogStart=1;atmosphere.fogEnd=22;atmosphere.horizontalFog=false;
        }
        renderer.setAtmosphere(atmosphere);
        std::vector<vc::RenderInstance> instances;
        for(const auto& view:living.views()) {
            const vc::voxel::Position p{static_cast<std::int64_t>(std::floor(view.position.x)),static_cast<std::int64_t>(std::floor(view.position.y+0.8)),static_cast<std::int64_t>(std::floor(view.position.z))};
            if(world.loaded(p)) instances.push_back({view.model,view.position,view.yaw,view.phase,{lighting.sky(p),lighting.block(p)}});
        }
        renderer.setInstances(instances);
        if(auto hud=ui.rebuild(registry,atlas,width,height,now))
            renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
        const auto updateMilliseconds=(vc::Window::seconds()-updateStart)*1000;
        const auto captureAt = options.frameLimit ? options.frameLimit - 1 : 60;
        const auto capture = !capturedScreenshot && !screenshot.empty() && frames >= captureAt;
        if (renderer.draw(camera, capture ? screenshot : std::filesystem::path{})) {
            ++frames; ++diagnosticFrames;
            if (capture) capturedScreenshot = true;
        } else vc::Window::idle();
        if (now - diagnosticStart >= 1.0) {
            const double fps = static_cast<double>(diagnosticFrames) / (now - diagnosticStart);
            std::ostringstream info;
            info << std::fixed << std::setprecision(1) << "VoxelCraft " VC_VERSION_STRING " | " << fps << " FPS / " << (fps > 0 ? 1000 / fps : 0)
                 << " ms | update/draw/upload ms "<<updateMilliseconds<<"/"<<renderer.metrics().drawMilliseconds<<"/"<<renderer.metrics().uploadMilliseconds<<" | calls "<<renderer.metrics().drawCalls
                 << " | xyz " << camera.position.x << ',' << camera.position.y << ',' << camera.position.z
                 << (player.flying ? " | Fly" : " | Walk") << (survivalMode?" | Survival":" | Creative")
                 << " | slot " << (survivalMode?survival.inventory.selectedSlot():inventory.selectedSlot())+1
                 << " | place " << (material==vc::voxel::air?"empty":registry.definition(material).identifier)
                 << " | target " << (hit ? registry.definition(world.block(hit->block)).identifier : "none")
                 << " | loaded " << world.chunks().size() << " / meshes " << renderer.meshCount()
                 << " / drawn " << renderer.drawnMeshCount() << " | triangles " << renderer.drawnTriangleCount() << '/' << renderer.triangleCount();
            if(streamer) info<<" | gen ms "<<streamer->generationMetrics().lastMilliseconds<<" | mesh ms "<<(backgroundMeshes?backgroundMeshes->metrics().lastMilliseconds:0)<<" | terrain pending "<<streamer->pendingCount()<<" / mesh pending "<<(backgroundMeshes?backgroundMeshes->pendingCount():meshQueue.pendingCount())
                <<" | cached edits "<<streamer->cachedCount()<<" / "<<streamer->cachedBytes()<<" bytes";
            info<<" | "<<day.clock()<<" | "<<vc::voxel::biomeName(generator.biomeAt(cameraBlock().x,cameraBlock().z));
            info<<" | creatures "<<living.creatureCount()<<" / loot "<<living.dropCount()<<" / drawn "<<renderer.drawnInstanceCount();
            window.title(info.str()); vc::log(info.str()); diagnosticStart = now; diagnosticFrames = 0;
        }
        if (options.frameLimit && frames >= options.frameLimit) break;
        if(input.save||(store&&now-lastSave>=60)) {
            try { save(); }
            catch(const std::exception& error) { vc::log(std::string("Save failed; world remains in memory: ")+error.what());notify("SAVE FAILED - SEE LOG",vc::NotificationSeverity::Error); }
            lastSave=vc::Window::seconds();last=lastSave;
        }
    }
    if (options.smoke && (frames < options.frameLimit || !restored || !capturedScreenshot)) throw std::runtime_error("Graphics smoke test interrupted before completion");
    if (options.smoke && (!smokeDestroyed || !smokePlaced || editCount != 2)) throw std::runtime_error("Interactive smoke test did not complete both edits");
    if (options.smoke && emptyCell && !smokeMeshRemoved) throw std::runtime_error("GPU mesh lifecycle smoke test was incomplete");
    if (options.smoke) {
        vc::log("Interactive smoke: seam edits and selection verified" + std::string(options.fly ? "" : "; jump/landing verified") +
            (emptyCell ? "; GPU mesh lifecycle verified" : "; mesh lifecycle skipped (no empty resident chunk)"));
    }
    vc::log("World edits: " + std::to_string(editCount));
    if(!savedOnExit&&!discardExit) save(true);
    if(discardExit) vc::log("Exited without saving by explicit Ctrl+Shift+Q request");
    vc::log("Rendered " + std::to_string(frames) + " frames; camera " + std::to_string(camera.position.x) + "," + std::to_string(camera.position.y) + "," + std::to_string(camera.position.z));
    renderer.shutdown();
    const auto validation = renderer.validationReport();
    if (validation.enabled) {
        vc::log("Vulkan validation: " + std::to_string(validation.errors) + " errors, " + std::to_string(validation.warnings) + " warnings (including shutdown)");
        if (validation.errors) throw std::runtime_error("Vulkan validation reported errors");
    }
    return 0;
}
}

int main(int argc, char** argv) {
    try {
        const auto options = vc::client::parseOptions(argc, argv);
        if (options.help) {
            std::cout << "VoxelCraft " VC_VERSION_STRING "\n--seed N --radius 1..6 --min-y N --max-y N --frames N --screenshot file.ppm --smoke --require-validation --fly\n"
                "--width N --height N --fullscreen / --windowed --vsync / --no-vsync\n"
                "--world-dir PATH (create/load a saved world) --no-save (temporary world)\n"
                "--connect HOST[:PORT] --name PLAYER --identity-file PATH (multiplayer; server controls world and game mode)\n"
                "--mods-dir PATH (load mod packs) --no-mods (retain saved mod data without loading packs)\n"
                "--survival (quantities, crafting, mining and health) --creative (infinite blocks and flight)\n"
                "--generator-version 1..3 (new worlds only; saved worlds retain their version)\n"
                "--time 0..1 (0 sunrise, .25 noon, .5 sunset, .75 midnight) --freeze-time\n"
                "--settings-file PATH --no-settings --fov 50..110 --sensitivity 0.0005..0.01 --invert-y --normal-y\n"
                "WASD/mouse: move/look. Space: jump. F: creative flight. F5: save. F10: settings. Left/right: mine/place. 1-9/wheel: hotbar. E: inventory. C: survival crafting. R: respawn after death. Escape: release mouse/close menu.\n"
                "Normal play saves to the user data directory; bounded runs are temporary unless --world-dir is specified.\n";
            return 0;
        }
        const int result = options.connectHost.empty()?run(options):vc::client::runRemote(options);
        vc::log("Clean shutdown");
        return result;
    } catch (const std::exception& e) {
        vc::log(e.what());
        return 1;
    }
}
