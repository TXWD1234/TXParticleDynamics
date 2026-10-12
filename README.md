# **Particle Dynamics**
**The Particle Physics Engine that reaches the limit of CPU computation.**

# Performance Profile
**Substep 1:**
- 7500 object
- 200 average fps

# Screenshot
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/screenshot1.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/screenshot2.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/screenshot3.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/screenshot4.png)

# Installation
## Build from source
*Make sure CMake, and a viable C++ compiler (recommand g++) is installed.*
**To build:**
1. Download and extract the source code
2. `cd` into the project directory, and execute the following commands:
```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd bin
```
**To execute:**
1. Execute the binary via `./TXProject`
2. *If the program logged the following, try `prime-run ./TXProject` or similar:*
```
Initializing Application...
Initializing GLFW...
Initializing GLAD...
glGetTextureHandleARB is null
glMakeTextureHandleNonResidentARB is null
[FatalError]: Failed to init GLAD
[FatalError]: Failed to init Application
```

## Pre-compiled binary
*The pre-compiled binary attached is targeting Arch Linux x86_64 (7.2.7-arch1-1).*
**To execute:**
1. Download and extract the binary archive
3. Execute the binary via `./TXProject`
6. *If the program logged the following, try `prime-run ./TXProject` or similar:*
```
Initializing Application...
Initializing GLFW...
Initializing GLAD...
glGetTextureHandleARB is null
glMakeTextureHandleNonResidentARB is null
[FatalError]: Failed to init GLAD
[FatalError]: Failed to init Application
```
