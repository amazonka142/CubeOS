# CubeOS Backlog (Issues)

## Priority Legend
- `P1`: core gameplay blockers (do first).
- `P2`: important features after core is stable.
- `P3`: polish and balancing.

## v0.2 Issues

### P1
- `CUBE-201` Empty start inventory.
  - Scope: remove pre-filled hotbar/inventory; new save starts empty.
  - Depends on: none.
  - Acceptance: new world starts with all slots empty.
  - Status: `Done` (implemented in current build).

- `CUBE-202` Add new block IDs and data.
  - Scope: add sand, gravel, wood, leaves, water, coal ore, iron ore, gold ore.
  - Depends on: none.
  - Acceptance: blocks can exist in world/save without crashes.
  - Status: `Done` (implemented in current build).

- `CUBE-203` Mountains generation pass.
  - Scope: extend terrain noise for high peaks and smoother foothills.
  - Depends on: `CUBE-202`.
  - Acceptance: terrain includes visible mountain ranges.
  - Status: `Done` (implemented in current build).

- `CUBE-204` Caves generation pass.
  - Scope: add 3D carve noise pass under surface.
  - Depends on: `CUBE-203`.
  - Acceptance: connected cave systems appear in generated chunks.
  - Status: `Done` (implemented in current build).

- `CUBE-205` Canyons generation pass.
  - Scope: add canyon mask and vertical carving.
  - Depends on: `CUBE-203`.
  - Acceptance: deep canyon structures appear naturally.
  - Status: `Done` (implemented in current build).

- `CUBE-206` Ore distribution.
  - Scope: coal/iron/gold spawn by depth bands and density settings.
  - Depends on: `CUBE-202`, `CUBE-204`.
  - Acceptance: ores are present with depth-based frequency.
  - Status: `Done` (implemented in current build).

### P2
- `CUBE-207` Tree generation.
  - Scope: place trunk + leaf canopies on valid surface blocks.
  - Depends on: `CUBE-202`, `CUBE-203`.
  - Acceptance: trees spawn without floating leaves/trunks.
  - Status: `Done` (implemented in current build).

- `CUBE-208` Water block behavior (basic).
  - Scope: source + short-range flow update (no advanced fluid sim).
  - Depends on: `CUBE-202`.
  - Acceptance: placed/generated water spreads and settles consistently.
  - Status: `Done` (implemented in current build).

- `CUBE-209` Meshing/render update for new blocks.
  - Scope: atlas tiles/material handling for all added block types.
  - Depends on: `CUBE-202`.
  - Acceptance: new blocks render with correct textures/colors.
  - Status: `Done` (implemented in current build).

### P3
- `CUBE-210` Worldgen tuning preset v0.2.
  - Scope: expose and tune constants for mountains/caves/canyons/trees/ores.
  - Depends on: `CUBE-203`..`CUBE-208`.
  - Acceptance: no extreme barren or overfilled worlds in smoke tests.
  - Status: `Done` (implemented in current build).

## v0.2.0-v0.2.2 Issues (Main Menu + World Creation)

### P1
- `CUBE-220` Main menu shell.
  - Scope: title screen with `Start`, `Settings`, `Quit` and input focus handling.
  - Depends on: none.
  - Acceptance: game opens to menu and can exit via menu.
  - Status: `Done` (implemented in current build).

- `CUBE-221` Create world flow.
  - Scope: world creation screen with world name + seed + confirm/cancel.
  - Depends on: `CUBE-220`.
  - Acceptance: player can create and enter a new world from UI.
  - Status: `Done` (implemented in current build).

- `CUBE-222` World preset support.
  - Scope: support at least `Classic Flat` and `Minecraft-style` presets.
  - Depends on: `CUBE-221`.
  - Acceptance: selected preset changes generated terrain profile.
  - Status: `Done` (implemented in current build).

- `CUBE-223` Persist world settings metadata.
  - Scope: save/load world creation options (preset + generation settings).
  - Depends on: `CUBE-221`, `CUBE-222`.
  - Acceptance: reopening world keeps selected settings.
  - Status: `Done` (implemented in current build).

### P2
- `CUBE-224` World settings panel v1.
  - Scope: cave density slider, ravine frequency slider, structures toggle.
  - Depends on: `CUBE-222`.
  - Acceptance: settings affect new world generation.
  - Status: `Done` (cave/ravine sliders and structures toggle now all affect generated chunks).

- `CUBE-225` Main menu settings screen.
  - Scope: graphics quality, sensitivity, audio placeholders, save/apply/reset.
  - Depends on: `CUBE-220`.
  - Acceptance: settings UI works and values persist.
  - Status: `Done` (implemented in current build).

- `CUBE-226` Start inventory mode option.
  - Scope: `Empty` vs `Creative test` start mode in world creation.
  - Depends on: `CUBE-221`.
  - Acceptance: spawn inventory matches selected mode.
  - Status: `Done` (implemented in current build).

### P3
- `CUBE-227` Menu polish and transitions.
  - Scope: subtle animations, hover/focus feedback, keyboard/controller navigation prep.
  - Depends on: `CUBE-220`, `CUBE-225`.
  - Acceptance: menu interaction is responsive and visually consistent.
  - Status: `Done` (animations/focus feedback present; controller menu navigation input wired).

## v0.2.1 Issues (Minecraft-Style Worldgen Expansion + In-Game UX)

### P1
- `CUBE-228` Multi-noise climate sampler parity pass.
  - Scope: move biome selection to full climate vector workflow (temperature/humidity/continentalness/erosion/depth/weirdness) with stable parameter ranges.
  - Depends on: `CUBE-223`, `CUBE-224`.
  - Acceptance: biome transitions are coherent and deterministic across chunk borders.
  - Status: `Done` (full climate-vector sampling with deterministic biome/climate maps across chunk borders).

- `CUBE-229` Density-router terrain shaping.
  - Scope: replace simplified height blending with router-style density composition (continents, peaks/valleys, erosion, detail).
  - Depends on: `CUBE-228`.
  - Acceptance: terrain produces recognizable plains, plateaus, ridges, and mountain chains without water-world bias.
  - Status: `Done` (router-style target height + density composition now drives terrain macro shape).

- `CUBE-240` Aquifer and underground fluid rewrite.
  - Scope: local water table model and deep fluid thresholds tuned to avoid surface flooding while preserving cave lakes.
  - Depends on: `CUBE-229`.
  - Acceptance: caves contain aquifer water pockets; surface remains predominantly land where climate suggests land.
  - Status: `Done` (aquifer level sampling now controls underground fluid fill with land-preserving clamps).

- `CUBE-241` Region-anchored structure starts/references.
  - Scope: region-based deterministic structure placement (village/mineshaft/temple scaffolding) with chunk intersection placement.
  - Depends on: `CUBE-228`.
  - Acceptance: structures spawn predictably per seed and stitch correctly across chunk boundaries.
  - Status: `Done` (structure starts are region-anchored and deterministic, with cross-chunk placement consistency).

- `CUBE-242` In-game pause menu flow.
  - Scope: `Esc` opens pause menu with `Continue`, `Settings`, `Main Menu` instead of immediate menu exit.
  - Depends on: `CUBE-220`.
  - Acceptance: gameplay pauses reliably and returns to world without state loss.
  - Status: `Done` (implemented in current build).

- `CUBE-243` Hotbar selection item-name toast.
  - Scope: show selected item name overlay for a short duration when slot selection changes.
  - Depends on: `CUBE-220`, `CUBE-226`.
  - Acceptance: switching hotbar slot displays readable item label for ~2 seconds.
  - Status: `Done` (implemented in current build).

### P2
- `CUBE-244` Structure piece sets and assembly rules.
  - Scope: define piece pools, placement constraints, and biome gating for structure variants.
  - Depends on: `CUBE-241`.
  - Acceptance: same structure type can appear in multiple valid layouts.
  - Status: `Done` (piece variants and biome-aware assembly rules produce multiple valid layouts per structure family).

- `CUBE-245` Carver event model parity.
  - Scope: deterministic worm/canyon event generation over regions with better continuity and thickness profiles.
  - Depends on: `CUBE-229`.
  - Acceptance: carvers travel naturally across multiple chunks without abrupt cutoff.
  - Status: `Done` (regional deterministic carver events now drive cross-chunk worm/canyon continuity).

- `CUBE-246` Surface rule graph.
  - Scope: configurable layered surface rules (coastline sand/gravel, snowline, mountain stone caps, desert depth rules).
  - Depends on: `CUBE-228`, `CUBE-229`.
  - Acceptance: biome-appropriate top/filler composition with less manual branching.
  - Status: `Done` (surface rule node graph now applies layered coastline/arid/mountain/snowcap/default composition).

- `CUBE-247` Feature pipeline enrichment.
  - Scope: per-biome feature sets for trees/vegetation/ores with weighted attempts and height logic closer to vanilla behavior.
  - Depends on: `CUBE-228`, `CUBE-246`.
  - Acceptance: each biome feels distinct in decoration and resource profile.
  - Status: `Done` (feature stage now uses biome/climate-weighted placement for trees, vegetation, ores, and shoreline detail).

### P3
- `CUBE-248` Worldgen debug overlays.
  - Scope: visual overlays for chunk borders, biome IDs, surface height, density slices, and aquifer level.
  - Depends on: `CUBE-228`..`CUBE-247`.
  - Acceptance: debugging tools can explain terrain outcomes in problematic seeds.
  - Status: `Done` (F3/F4 debug overlay shows biome/climate, chunk/local border metrics, surface/aquifer, and density slices).

- `CUBE-249` Seed parity and tuning pass.
  - Scope: run deterministic seed suite and tune thresholds toward Minecraft-like macro results.
  - Depends on: `CUBE-228`..`CUBE-248`.
  - Acceptance: no major regressions in spawn quality, terrain diversity, or structure distribution.
  - Status: `Done` (deterministic worldgen regression suite added with seed metrics and threshold gates).

## v0.2.3 Issues (Coins + Upgrades)

### P1
- `CUBE-230` Coin entity + pickup.
  - Scope: add world coin objects, collision pickup, and HUD coin counter.
  - Depends on: `CUBE-220`.
  - Acceptance: coins can be collected and counter increases in session.

- `CUBE-231` Persistent coin balance.
  - Scope: save/load coin balance with world/profile data.
  - Depends on: `CUBE-230`.
  - Acceptance: coin balance persists after restart.

- `CUBE-232` Main menu upgrades screen.
  - Scope: add `Upgrades` button and upgrade list UI in main menu flow.
  - Depends on: `CUBE-220`.
  - Acceptance: player can open upgrades menu from title screen.

- `CUBE-233` Underwater duration upgrade.
  - Scope: implement level-based upgrade that increases underwater time.
  - Depends on: `CUBE-231`, `CUBE-232`.
  - Acceptance: purchased upgrade clearly extends underwater survivability.

### P2
- `CUBE-234` Upgrade economy rules.
  - Scope: price curve, max levels, and insufficient-funds handling.
  - Depends on: `CUBE-232`.
  - Acceptance: buying logic blocks invalid purchases and updates UI state.

- `CUBE-235` Coin spawn distribution.
  - Scope: scatter coins in traversable world regions with anti-farm cooldown.
  - Depends on: `CUBE-230`.
  - Acceptance: coins are discoverable but not infinitely farmable at spawn.

- `CUBE-236` Upgrade persistence model.
  - Scope: persist purchased upgrade levels and apply on world load.
  - Depends on: `CUBE-233`, `CUBE-234`.
  - Acceptance: upgrades remain purchased and active after relaunch.

### P3
- `CUBE-237` Progression balance pass.
  - Scope: tune coin rates and upgrade costs for first 30-60 minutes.
  - Depends on: `CUBE-230`..`CUBE-236`.
  - Acceptance: progression is neither grindy nor trivial in playtests.

## v0.3 Issues

### P1
- `CUBE-301` Item + tool type system.
  - Scope: add tool items (wood/stone/iron pickaxe, axe, shovel).
  - Depends on: `CUBE-202`.
  - Acceptance: tool items can be stored/selected in inventory.

- `CUBE-302` Tool effectiveness rules.
  - Scope: map block families to effective tool types.
  - Depends on: `CUBE-301`.
  - Acceptance: correct tool mines noticeably faster.

- `CUBE-303` Tool durability.
  - Scope: durability decrease per block break and break-at-zero behavior.
  - Depends on: `CUBE-301`.
  - Acceptance: tools degrade and disappear at zero durability.

- `CUBE-304` Ore drop rules by tool tier.
  - Scope: enforce minimum tier for ore drops.
  - Depends on: `CUBE-302`, `CUBE-303`.
  - Acceptance: wrong tool/tier does not yield ore drop.

### P2
- `CUBE-305` UI support for tool durability.
  - Scope: durability bar/number in hotbar and inventory.
  - Depends on: `CUBE-303`.
  - Acceptance: durability is visible and updates live.

- `CUBE-306` Basic crafting or debug grant path.
  - Scope: either minimal crafting recipes or debug command for tools.
  - Depends on: `CUBE-301`.
  - Acceptance: player can obtain and use all tool tiers.

- `CUBE-308` Render-layer split and transparent sorting.
  - Scope: split chunk-section mesh output into `solid` / `cutout` / `translucent` layers and add per-section translucent sort path.
  - Depends on: chunk-section streaming pipeline in v0.2.x.
  - Acceptance: transparent blocks/plants render in stable order without major artifacts during movement.

- `CUBE-309` Aggressive section culling (frustum + occlusion).
  - Scope: add per-section frustum culling and occlusion-aware rejection to reduce overdraw and draw-call load.
  - Depends on: `CUBE-308`.
  - Acceptance: hidden/off-camera sections are skipped consistently; measurable draw-count reduction in dense scenes.

- `CUBE-310` GPU upload batching and sync modernization.
  - Scope: replace coarse update sync with staged/batched chunk-section uploads and non-blocking lifetime management.
  - Depends on: `CUBE-308`.
  - Acceptance: chunk streaming under player movement avoids visible frame hitches from upload synchronization.

### P3
- `CUBE-307` Mining feel polish.
  - Scope: tweak break duration curves by tool/material.
  - Depends on: `CUBE-302`, `CUBE-303`.
  - Acceptance: mining pace feels consistent across progression.

## v0.4 Issues

### P1
- `CUBE-401` Chunk seam cleanup for terrain and caves.
  - Scope: ensure deterministic border continuity across chunk boundaries.
  - Depends on: v0.2 worldgen issues.
  - Acceptance: no obvious cracks/cutoffs on chunk borders.

- `CUBE-402` Water propagation stability.
  - Scope: prevent runaway updates and performance spikes.
  - Depends on: `CUBE-208`.
  - Acceptance: water updates remain stable in stress test scenes.

### P2
- `CUBE-403` Spawn/rarity balancing pass.
  - Scope: ore rates, tree density, canyon frequency.
  - Depends on: v0.2 complete.
  - Acceptance: progression not starved and not over-abundant.

- `CUBE-404` Early progression balancing.
  - Scope: break speed, durability, drop counts.
  - Depends on: v0.3 complete.
  - Acceptance: first 10-15 minutes of play feel coherent.

### P3
- `CUBE-405` Playtest checklist and regression pack.
  - Scope: create repeatable smoke tests for worldgen + inventory + mining loop.
  - Depends on: v0.2-v0.4 core issues.
  - Acceptance: checklist runs clean before each release.
