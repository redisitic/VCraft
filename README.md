# VCraft

A Minecraft-like voxel engine built with C++20 and Vulkan, featuring fully hardware-accelerated per vertex ray-traced lighting.

## Features

* ** Per Vertex Hardware Ray Tracing**:
  Utilizes Vulkan Acceleration Structures (TLAS/BLAS) and Ray Queries in Compute Shaders to evaluate per-vertex lighting.
  * **Simple Lighting Mode**: Crisp sun shadows, ambient occlusion, and dynamic point lights (torches) with hard shadow rays.
  * **ReSTIR GI Mode**: Advanced Global Illumination using Reservoir Spatio-Temporal Importance Resampling (ReSTIR). Features multi-sample sky-light evaluation, realistic one-bounce light bleeding, and temporal accumulation (EMA) for real-time denoised path tracing.
* **Dynamic Colored Lights**: Placeable torches acting as physical point lights that participate in GI bouncing.

## Requirements

* **OS**: Windows 10/11 (or Linux with appropriate windowing support)
* **GPU**: A Vulkan 1.2+ capable GPU with Hardware Ray Tracing support (e.g., NVIDIA RTX series, AMD RX 6000 series or newer).
* **Vulkan SDK**: 1.3+ installed.
* **Compiler**: Visual Studio 2022 / MSVC (C++20 support).
* **Build System**: CMake 3.10+.

## Build Instructions

To build the project using CMake via the command line:

```bash
# Generate build files
cmake -B build -S .

# Build the Release executable
cmake --build build --config Release
```

Once built, you can run the executable from the `build/Release` directory or directly via your IDE. The project automatically handles copying compiled SPIR-V shaders (`.spv`) and the texture atlas to the target directory.

## Controls

* **W, A, S, D**: Move
* **Space**: Jump
* **Left Click**: Break block
* **Right Click**: Place block
* **Scroll Wheel**: Select hotbar slot
* **`/` (Forward Slash)**: Open Console

## Console Commands

Press `/` in-game to open the console and type commands:

* `/time set <0.0-24.0>` - Set the time of day (0.0 = midnight, 6.0 = sunrise, 12.0 = noon, 18.0 = sunset).
* `/lighting simple` - Switch to standard ray-traced shadows + baked AO.
* `/lighting restir [samples] [bounces]` - Switch to ReSTIR path-traced GI.
  * `samples`: Number of hemisphere rays shot per vertex per frame (default 1). Higher = smoother, slower.
  * `bounces`: Number of indirect GI bounces (0 disables bounce light, >0 enables sun/torch color bleeding).
* `/give torch [color] [count]` - Gives you colored torches.
  * Colors can be hex (`#FF00FF`), names (`red`, `blue`, `cyan`), or color temperature (`8000K`).
* `/hud on|off` - Toggles the crosshair and hotbar for clean screenshots.

## Screenshots

### Per Vertex Lighing

<img width="1273" height="718" alt="0" src="https://github.com/user-attachments/assets/437b8f17-86dd-4244-864a-7fab67bffd2e" />


### Restir Bounce Lighting

<img width="1271" height="715" alt="1" src="https://github.com/user-attachments/assets/62e4784b-245a-4dcd-87da-c7a8de96ae85" />


### Restir Off

<img width="1272" height="720" alt="2" src="https://github.com/user-attachments/assets/56261f27-12b9-4fb4-9916-eb20e73d2850" />


### Restir On

<img width="1275" height="718" alt="3" src="https://github.com/user-attachments/assets/f9f8bf2e-2640-4aba-a5a3-3077acaff037" />


### Point Shadows

<img width="1277" height="717" alt="Screenshot 2026-03-07 041204" src="https://github.com/user-attachments/assets/0245d2ee-25dc-44b9-ad5e-bde5f6a69573" />


### Minecraft Style Smooth Lighting

<img width="1277" height="712" alt="4" src="https://github.com/user-attachments/assets/61149630-7efc-493c-a51c-26a84b7e96c2" />
