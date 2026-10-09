## 2026-10-08 23:07:38:<br>Category: Development Documentation<br>Topic: Project Overview
Welcome to the development of TXParticleDynamics (TXPD), the particle physics engine targeting to reach the limit of the CPU computation.
It is designed for the absolute maximum raw CPU performance, carrying the only pursuit of simulating massive quantity of particle collisions, utilizing (one of) the fastest language known to man kind: C++, custom OpenGL rendering pipeline made from scratch, and various intrinsical optimizations, namely Parallel Processing, SoA, SIMD to achieve the absolute extremity of the hardware.
The TXParticleDynamics project is backed by [TXLib](https://github.com/TXWD1234/TXLib), a C++ general purpose library designed for performance and versatility. TXLib will handle graphics, data management, math and a variaties of infrastructures of the project, while TXParticleDynamics focus on physics logic and hyper optimization.
The target of TXParticleDynamics is to handle 1,000,000 object in 60 UPS with *20 threads running 4.7GHz along with 24GB of RAM (this is just the specification of my machine)* with merely CPU computation, without Compute Shaders.
The development of the project must be relentless, no amount of implementation difficulty can resist the pursuit of extremity.
*The only limitation, is the machine, not the software.*

## 2026-10-08 23:27:52:<br>Category: Development Documentation<br>Topic: Development Overview
### Development environment
OS: Arch Linux
Language: C++
*Managed with CMake.*
Code Editor: Visual Studio Code
Developer: [TX_Jerry](https://github.com/TXWD1234)

### Development Schedule
Given that TXLib (which is also developed and maintained by myself) is the vital dependency of the project, some infrastructure is required to be completed before the actual development of the project can begin, namely the following:
- TXData Overlay Pattern Refactor
- TXGraphics Refactor
- TXMath V2 Refactor
After above all completed, in which are targeted in early November, the development of the physics engine will begin.