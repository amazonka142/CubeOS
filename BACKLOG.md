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

- `CUBE-202` Add new block IDs and data.
  - Scope: add sand, gravel, wood, leaves, water, coal ore, iron ore, gold ore.
  - Depends on: none.
  - Acceptance: blocks can exist in world/save without crashes.

- `CUBE-203` Mountains generation pass.
  - Scope: extend terrain noise for high peaks and smoother foothills.
  - Depends on: `CUBE-202`.
  - Acceptance: terrain includes visible mountain ranges.

- `CUBE-204` Caves generation pass.
  - Scope: add 3D carve noise pass under surface.
  - Depends on: `CUBE-203`.
  - Acceptance: connected cave systems appear in generated chunks.

- `CUBE-205` Canyons generation pass.
  - Scope: add canyon mask and vertical carving.
  - Depends on: `CUBE-203`.
  - Acceptance: deep canyon structures appear naturally.

- `CUBE-206` Ore distribution.
  - Scope: coal/iron/gold spawn by depth bands and density settings.
  - Depends on: `CUBE-202`, `CUBE-204`.
  - Acceptance: ores are present with depth-based frequency.

### P2
- `CUBE-207` Tree generation.
  - Scope: place trunk + leaf canopies on valid surface blocks.
  - Depends on: `CUBE-202`, `CUBE-203`.
  - Acceptance: trees spawn without floating leaves/trunks.

- `CUBE-208` Water block behavior (basic).
  - Scope: source + short-range flow update (no advanced fluid sim).
  - Depends on: `CUBE-202`.
  - Acceptance: placed/generated water spreads and settles consistently.

- `CUBE-209` Meshing/render update for new blocks.
  - Scope: atlas tiles/material handling for all added block types.
  - Depends on: `CUBE-202`.
  - Acceptance: new blocks render with correct textures/colors.

### P3
- `CUBE-210` Worldgen tuning preset v0.2.
  - Scope: expose and tune constants for mountains/caves/canyons/trees/ores.
  - Depends on: `CUBE-203`..`CUBE-208`.
  - Acceptance: no extreme barren or overfilled worlds in smoke tests.

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
  - Status: `Partial` (cave density and ravine frequency affect generation; structures toggle is UI + persistence only for now).

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
  - Status: `Partial` (animations and focus feedback are present; controller navigation prep remains open).

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
