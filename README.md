# Angry Birds Prototype (SFML + Box2D)

A small 2D physics game inspired by **Angry Birds**, written in **C++**. It uses **SFML 3** for graphics, input, and timing, and **Box2D 3** for rigid-body physics (gravity, collisions, impulses).

This repository contains the **source** (`Game.cpp`). You compile it yourself; runtime DLLs are not committed (see [Build](#build)).

---

## Table of contents

1. [What you can do in the game](#what-you-can-do-in-the-game)
2. [How to play](#how-to-play)
3. [Game rules (levels, pigs, lives)](#game-rules-levels-pigs-lives)
4. [Damage and materials](#damage-and-materials)
5. [Level initialization and stability](#level-initialization-and-stability)
6. [Technical architecture](#technical-architecture)
7. [Build](#build)
8. [Run on Windows](#run-on-windows)
9. [Pause menu](#pause-menu)
10. [Project layout](#project-layout)
11. [Dependencies](#dependencies)
12. [License / credits](#license--credits)

---

## What you can do in the game

- **Drag and launch** a red bird from a slingshot-style anchor.
- See an **aim line** and a **predicted trajectory** while dragging.
- **Destroy green pigs** (targets) using the bird and falling blocks.
- Play across **multiple levels** with different block layouts and pig counts.
- Use up to **four birds per level** (lives). If you run out, the level **restarts**.
- **Pause** the game and **restart** the current level or **quit**.

---

## How to play

| Action | Input |
|--------|--------|
| Aim / pull bird | Hold **left mouse button** on the bird and drag |
| Fire | Release **left mouse button** |
| Pause / resume | **Esc** |
| Restart level (while paused) | **R** |
| Quit (while paused) | **Q** |

The window title shows useful state, for example:

- Current **level number**
- How many **pigs** are still alive
- How many **birds** you have left for that level

---

## Game rules (levels, pigs, lives)

### Levels

- The game defines several **levels** in code. Each level specifies:
  - A list of **pig spawn positions** (up to four pigs per level in the current design).
  - A list of **blocks** (position, size, and **material**).
- **Win condition:** eliminate **all pigs** on the level.
- When you win, the game advances to the **next** level (and loops after the last one).

### Birds (lives)

- Each level starts with **up to 4 birds**.
- When you **release** a shot, you spend one bird from your remaining count.
- After a bird stops moving or leaves the play area, a **new bird** is spawned at the slingshot if you still have birds left.
- If you have **no birds left** and the level is not cleared, the **same level reloads** (try again).

### Pigs

- Pigs are drawn as **green circles**.
- Each pig has **health** (see [Damage and materials](#damage-and-materials)).
- The **red bird** is designed to **one-shot** a pig on contact (instant kill), in addition to normal hit-event damage.

---

## Damage and materials

### Pig health

- Pigs start with **200 HP** (constant in code: `kPigMaxHealth`).

### Material tiers (block → pig damage)

When a **block** hits a pig hard enough, damage depends on the block’s **material**:

| Material | Relative damage |
|----------|------------------|
| **Stone** | Highest |
| **Wood** | Medium |
| **Ice** | Lowest |

The formula uses Box2D **hit events** (`approachSpeed`) scaled by a material multiplier. This is tuned for gameplay, not real-world physics.

### Bird damage

- The bird does **not** have health.
- Bird-on-pig contact is treated as a **guaranteed kill** for that pig (very large damage value in code).

### Warmup after level load

- After a level loads, there is a short **damage warmup** window. During this time, **only the bird** can damage pigs from hit events; **blocks** do not apply settling damage. This avoids the pig dying instantly from spawn jitter before stabilization completes.

---

## Level initialization and stability

Real physics means dynamic bodies can **settle**, **slide**, or **collapse** if they overlap or are slightly misaligned.

This project aims for a **stable “built” look** when the level **first appears**:

1. Blocks and pigs are created as **dynamic** bodies in their designed positions (with Y adjusted so they rest on the ground plane where intended).
2. The world runs a **hidden pre-simulation** for a bounded number of steps until movement falls below a threshold for a sustained period.
3. Then linear and angular velocities are **zeroed** and bodies are put to **sleep** so the visible first frame looks **still and settled**.

After that, normal gameplay applies: impulses from the bird and collisions wake bodies and the tower can fall apart realistically.

### Bounds

- **Ground** and **side walls** keep most action on screen.
- A **ceiling** prevents the bird from flying upward without limit.

---

## Technical architecture

### Single-file structure

- **`Game.cpp`** contains the entire game: constants, helpers, entity creation, level data, main loop, rendering, and game rules.

### Coordinate systems

- **Pixels (SFML):** window drawing, mouse positions, circle/rectangle sizes.
- **Meters (Box2D):** physics positions, velocities, impulses.
- A constant **`kPixelsPerMeter`** converts between them for rendering and input.

### Main loop (every frame)

1. **Poll events** (close window, keyboard, mouse).
2. If not paused: **step** Box2D, read **contact hit events** for pig damage, check win/lose/bird respawn rules.
3. **Sync** drawable shapes to physics body transforms.
4. **Draw** background, ground, slingshot, aim helpers, bird, pigs, blocks.

### Libraries

- **SFML 3:** `sf::RenderWindow`, shapes, events (`std::optional` + `Event` subtypes), `sf::Clock`.
- **Box2D 3:** C-style API with **IDs** (`b2WorldId`, `b2BodyId`, `b2ShapeId`), `b2World_Step`, shape creation (`b2CreatePolygonShape`, `b2CreateCircleShape`), `b2World_GetContactEvents` for hits.

### Tags (`BodyTag`)

Small structs attached via `b2BodyDef::userData` identify each body as **bird**, **pig**, or **block**, and carry **material** and **pig index** for multi-pig damage routing.

---

## Build

### Prerequisites (Windows, recommended: MSYS2 MinGW64)

- **MSYS2** with the **mingw-w64-x86_64** toolchain
- Packages (install via `pacman` in the MSYS2 environment), for example:
  - `mingw-w64-x86_64-gcc`
  - `mingw-w64-x86_64-sfml`
  - `mingw-w64-x86_64-box2d`

### Compile (PowerShell or CMD)

From the project directory (folder containing `Game.cpp`):

```text
C:\msys64\mingw64\bin\g++.exe -std=c++20 Game.cpp -o Game.exe ^
  -I C:\msys64\mingw64\include -L C:\msys64\mingw64\lib ^
  -lsfml-graphics -lsfml-window -lsfml-system -lbox2d
```

Adjust paths if your MSYS2 install is not `C:\msys64`.

---

## Run on Windows

1. Build `Game.exe` (see above).
2. Either:
   - Add `C:\msys64\mingw64\bin` to your **PATH** for that session, then run `Game.exe`, or  
   - Copy the required **MinGW DLLs** next to `Game.exe` (SFML, Box2D, `libstdc++`, `libgcc`, and their dependencies such as FreeType, etc.).

If Windows reports a missing DLL, use **Dependency Walker**-style tools or copy DLLs from `mingw64\bin` until the executable starts.

---

## Pause menu

The pause UX is intentionally simple and **does not require a font file**:

- While paused, the **window title** shows the key bindings for resume, restart, and quit.

For an in-window menu with text buttons, you would add a `.ttf` font and use `sf::Text` (not included in this repo).

---

## Project layout

```text
Multimedia Game/
├── Game.cpp      # Full game source
├── README.md     # This file
├── .gitignore    # Ignores exe, DLLs, IDE junk
└── (optional) Game.exe + DLLs after local build — not in git
```

---

## Dependencies

| Library | Role |
|---------|------|
| **SFML 3** | Window, rendering, input, timing |
| **Box2D 3** | 2D rigid-body physics |
| **C++20 compiler** | `std::optional`, designated initializers, etc. |

---

## License / credits

- This is a **student / prototype** project for learning game programming.
- **SFML** and **Box2D** are separate projects with their own licenses; see their official sites for terms.

If you publish a derivative, comply with SFML and Box2D license requirements and give appropriate attribution.
