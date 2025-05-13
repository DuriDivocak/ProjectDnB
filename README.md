#  Real-Time Audio Visualizer (ProjectM DnB Mod)

This project is a customized iteration of the open-source [ProjectM](https://github.com/projectM-visualizer/projectm) visualizer, tailored specifically for real-time visualization of **drum and bass (D\&B)** music. It integrates advanced audio feature extraction and custom visual presets designed to improve synchronization and visual feedback during live music playback.

---

##  Based On

- [**ProjectM**](https://github.com/projectM-visualizer/projectm) – main visualization engine  
- [**frontend-sdl-cpp**](https://github.com/projectM-visualizer/frontend-sdl-cpp) – SDL-based standalone frontend  
- [**Presets – Cream of the Crop**](https://github.com/projectM-visualizer/presets-cream-of-the-crop) – original preset collection used as a base

---

##  What’s New

-  Modified analysis engine: added real-time audio features (RMS, Spectral Flux, simplified harmonic separation)
-  Custom visual presets tailored to various D\&B subgenres
-  Feature-driven modulation: visuals respond to rhythm, bass energy, and melodic structures

---

##  How to Run

###  Option 1: Use Prebuilt Binaries

Prebuilt application is available in the following folder (after extracting from the provided ZIP):

Prebuild binaries can be found in:

https://drive.google.com/drive/folders/1ETki4r6mA11UXDNVvCyNSGQgyUzVBC0B?usp=sharing

frontend-sdl-cpp\build\src\Release\projectMSDL.exe

1. Run the executable
2. Set the preset folder in the application's settings
3. Presets are located in:

projectm-assets\presets-cream-of-the-crop\demo-presets

> ℹ️ Original full preset pack (9000+): [Presets GitHub](https://github.com/projectM-visualizer/presets-cream-of-the-crop)

---

###  Option 2: Build from Source

If you want to build or modify the project:

####  Prerequisites

- Visual Studio 2022
- [CMake](https://cmake.org/) (v3.20+)
- [vcpkg](https://github.com/microsoft/vcpkg) (used for dependency management, e.g., SDL2, FFTW3)

Ensure `cmake`, `vcpkg`, and your compiler are available in your system PATH.

####  Build Instructions

From `C:\dev\projectm`, run:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:/dev/projectm-install
cmake --build build --config Release --parallel
cmake --install build --config Release
```

This will generate the following output:
C:\dev\projectm-install\bin\
- projectM-4.dll
- projectM-4-playlist.dll

Copy these DLLs manually into:
frontend-sdl-cpp\build\src\Release\

---