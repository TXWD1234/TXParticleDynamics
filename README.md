# **Particle Dynamics**
**The Particle Physics Engine that reaches the limit of CPU computation.**

# Performance Profile
**Substep 1:**
- 7500 object
- 200 average fps

# Screenshot
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/cluster.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/galaxy.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/galaxy-2.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/spiral.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/centrifuge.png)
![Screenshot](https://github.com/TXWD1234/TXParticleDynamics/raw/main/docs/readme-assets/centrifuge-2.png)

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

## Adjust the config and Play around with the Lua API
The engine exposes a set of Lua APIs that can be called in a lua script. Which means you can write you own script and experiment with the engine. Here's how to do it:
1. Create a new foldering in `demo/`. You can also just copy one of the existing demos.
2. Make sure there are 3 files: `configure.lua`, `init.lua` and `runtime.lua` in the folder. It's okay for a file to be empty
3. Edit entry `"script"` in `config/config.json` to your demo's path. It is the relative path from project root to the demo.
4. Take reference to the `api.lua` file and other existing demos.
5. Start writing your custom script!