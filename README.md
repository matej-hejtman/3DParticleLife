# Particle Life 3D

3D real-time Particle Life simulation written in modern C++ and OpenGL 4.3
compute shaders.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/particle_life_3d
```

The first CMake configure downloads GLFW, GLAD, Dear ImGui and nlohmann/json
through `FetchContent`.

## Controls

- Left mouse drag: look around
- W/A/S/D: move forward/left/back/right
- Q/E: move down/up
- Shift: faster movement
- Ctrl: slower movement
- Mouse wheel: change movement speed
- Home: reset camera
- Space: pause/resume
- R: reset simulation
- Esc: close

## Notes

The simulator is GPU-only for the particle update path. It uses a fixed-capacity
uniform grid on the GPU to avoid all-pairs interaction. The 1M particle mode is
experimental and intentionally exposes grid resolution and bucket capacity so
performance and quality tradeoffs can be measured.
