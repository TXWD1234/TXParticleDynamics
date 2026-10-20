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

## 2026-10-09 19:40:27:<br>Category: Development Documentation<br>Topic: Grid System Design
*This is just a brief explaination of the GridSystem's role. The actual logic explaination is going to be in the formal documentation instead of this random report.*
The Grid System is a vital performance optimization of the engine, serves for the broad phase filtering process, reduce the exponential time complexity growth to linear growth.
The grid system adopted by this engine is uniform grid system with some optimizations applied.
It is a giant 2D array of grid cells, each grid cell stores the IDs (ECS index) of the particles in it's range respectively.
The following described the grid update logic that is performed every iteration to update the grid system against the new object position.

---
There are 3 phases of the grid system (gs) update:
- Collect object distribution information
- Compose grid index array
- Compile final grid system array

### Variables and Settings
```cpp
N = 1000000 // object count, as well grid cell count
T = 20 // thread count
```
*Performance analyzation and notes are in italic.*

### Collection
*Parallel: true*
*SIMD: true*
Buffers:
- (Input) Position Array (`vec2[N]`)
- (Output) Object Distribution Array (`u8[T][N]`) *The capacity of `255` of `u8` still yields a ton of excess space for the purpose array. Each element is only for a single cell in a single thread, in which the theoretical maximum count of object a cell can hold is 4. DevNote: reduce to u4.*

Iterate through the Position Array and calculate the grid index.
This process is split into threads, each thread talk `1/T` of the entire Position Array, and output to their respective partition in Object Distribution Array.
The output format is: `arr[index]++` increment the counter at the respective coordinate.
*Threads are lock free since no data write overlaps; Each thread only access it's designated buffer partition, avoiding Read-For-Ownership.*

At the end of this stage, Object Distribution Array should be produced with information of the amount of object each cell contain across all threads.

### Composition
*Parallel: true*
*SIMD: partial*
Buffers:
- (Input) Object Distribution Array
- (Output) Grid Index Array (Arena: `{MasterLookupTable: u32[N], EntryList: {embeddedLookupTable: u8[T], entries: u32[<overlapping-thread-count>]}[<populated-cell-count>]}`)
- (Intermediate) Cell Population Array `u16[N/16]` *u16 because a cache line is 64 bytes, which is 16 u32 (might be adjusted later for SIMD)*

The process of composing Grid Index Array has 2 phases, namely:
- Fence Collection
- EntryList Construction
The entire purpose of Grid Index Array is to optimize for the execution speed of the third major phase: Compilation.

#### Fence Collection
Collect meta data for further lock free, relocation free EntryList Construction.
This phase determinds the location of all empty cells, as well as the write head position of each thread stride.

Threads are now splited against N associated with the Object Distribution Array, each targeting a stride of `u32` across all `T` partitions. (which means un-contiguous, multi-dimensional access) (This defines "thread partition" in this phase: the partitions of buffer that are assigned to each thread.) Now each thread partition consist all Object Distribution Information of grid cells in `1/T` of the entire grid system.
Each thread cast each u32 value into boolean value, then mask them together to create one integer bitmask expressing the population of each cell, stored in Cell Population Array.
*Cell Population Array is also partitioned, each integer element are exactly corresponding to same amount of cells, therefore no write conflict present. Reading from Object Distribution Array is completely SIMDable.*
Then each of the other `T` partitions, being processed by the same thread, is similarly masked, and combined with the previous masks via or.
This produces the final product of Cell Population Array, representing the population of all `N` grid cells.
During the above process, all values in one thread's partition are sumed, outputing in intermediate buffer `u32 objectCount[T]`, representing the count of objects in all cells of that thread partition.

At the end of the phase, all one bits in Cell Population Array are to be counted in isolation of thread partition, result in `u32 populatedGridCount[T]`, which represents the count of populated cells of each thread partition. Then it is combined with `objectCount` in formula: `populatedGridCount[i] * ceil(T/4) + objectCount[i] * 1` to create the final result of Fence Collection Stage: `u32 fenceArray[T]`, the write head index of each thread in EntryList Construction phase, in unit of u32.
*Notice that the fences produced with this formula is not certainly 100% compact, because in edge cases where two object fall in the same grid in the same thread partition, the final entry count in Grid Index Array's EntryList's entries will be less than expected (calculated by `objectCount[i]`). But since that case is relatively rare, and optimizing it would trade more valuable property than a compromise on absolute compactness, this is considered as minor and is accepted.*

#### EntryList Construction
This is the actual process of composing the Grid Index Array.

All threads now are still targeting the same thread partition from previous phase, with the additional write head `fenceArray[i]` in Grid Index Array's EntryList.
Each thread are to iterate through their assigned thread partition in Object Distribution Array in the sequence of cells (which means multi-dimensional stride access), and create a cell entry in Grid Index Array for each entry:
- Register in Grid Index Array's MasterLookupTable: `lut[gridIndex] = writeHead`
- Construct an entry in Grid Index Array's EntryList
  - collect all thread slices of the cell and increment the object counter by the value of the respective thread slice for each entry in `entries` (the expecting value is 1)
  - increment the write head
The object counter root is from a psa constructed by `objectCount`.
Use Cell Population Array to skip empty partitions. This may introduce branch, but it saves `T` access and `T` operations in the Object Distribution Array, as well as the lookup table registeration, and the cell entry that otherwise would cost more than a branch both in present and the future.*

This process creates the Grid Index Array for the third major phase: Compilation to determind the write head of any specific thread for any specific cell, thus completely avoiding relocation.
The interface for Compilation is: `gridIndexArray[gridIndex]` to get the index of the cell entry from root of the arena, use that index to find the root of the cell entry, then use the embedded lookup table to get the write head for the specific thread.
*2 indirections, all within the same buffer, the second one is almost free since the destination and the embedded lookup table is almost guaranteed to be in the same cache line.*

### Compilation
*Parallel: true*
*SIMD: true*
Buffers:
- (Input) Position Array
- (input) Grid Index Array
- (Output) Grid System Array

Similar to phase 1 Collection, threads are again splited among the Position Array, each assigned to `1/T` of the Array, and calculating the grid index of each position.
But this time, after the grid index is found, use that index along with the thread id of the thread to find the exact write head in Grid Index Array, and then write the object id to that address, and increment the write head.
*Becasue Grid Index Array is optimized for memory footprint, it can fit in L3 cache, which means a very fast lookup for this stage.*

Finally, the grid system array is now produced.

## 2026-10-10 11:12:26:<br>Category: Development Report<br>Topic: Designing Grid System
Okay so, first I must admit: the above algorithm is way too over-complicated and is fundamentally flawed.
The biggest flaw is: During the 3rd phase: Compilation, the write operation into the final Grid System Array is completely random, which means 2 threads can write to adjacent element simultaneously, which means potential false sharing cache line and thus triggering Read-For-Ownership.
Another flaw is the memory footprint of the Object Distribution Array is too big to endure. As the example of `N = 1,000,000` and `T = 20`, it is a whole 20MB buffer need to fit in the cache line, combined with every other buffers, they are a little excessive even for the L3 cache of the most powerful machines.
The unfortunate thing is, I realized that when I almost done writing the documentation, and at that point it is going to excruciating for me to not complete the documentation, so the flawed algorithm eventually lasted.

Apart from the flawed result, I actually think the design of the algorithm is pretty elegent. The core part of my optimization was phase 2: Composition, with the target of reducing the memory footprint of the Grid Index Array from the naive `u32[T][N]` to something smaller. And I'm proud to say that this specific target was achieved flawlessly. It is just I am not wise and sensitive enough to identify the real flaw of the entire algorithm.

Finally, I want to conclude this report with the lesson I've learned: always examine the algorithm thoroughly before putting real effort to it.
But to be fair, it is actually not too late, since I made the incredible correct decision of writing the documentation first before actually implementing the algorithm, which takes much more effort.
*Another lesson is to tell myself to stop when I feel like the algorithm is getting way too complex.*

## 2026-10-11 16:31:56:<br>Category: Development Report<br>Topic: TXLib: TXData: Overlay Pattern: Refinement to previous architecture and Bug fixes
- Add OverlayBaseIntrinsicsAccessor to grant OverlayAlias access to intrinsics of any OverlayBase
- Add Reallocation Policy to buffer_trait for OverlayBufferExpansionResize and OverlayMMW's copy semantics to respect custom constraint of buffer, such as pow2
- Fix OverlayMMW passing `TArgs` entirely into Overlay without taking out the allocator arguemnt at the end
- Fix `s.self()` ambiguity in CRTP interface helpers's deducting this functions
- Unify Allocator representation across components: store as value, copy when rebind, reference whenever possible
- Flatten the `overlaySetBufferState` call in `OverlayMMW::copyReallocate_impl` to fix size value stale bug
- Various of other small bug fixes

## 2026-10-11 21:32:37:<br>Category: Development Report<br>Topic: Implement Draw Circle
*It's been a while since the last time I did anything regarding rendering, so there are a lot things I need to pick up.*
The logic of drawing circle with GPU is quite elegant. You act like you are drawing a square, passing teh ordinary 4 vertices to the vertex shader (which process every single vertex of the mesh) as usual. This made the API for draw normal sprites and circles uniform. And then in the fragment shader (which process every single pixel in the range of the vertices), you calculate the distance between that pixel and the center of the circle, scale the alpha value with the distance where (conceptually) any pixel that has distance bigger then the radius become transparent.
*I was really trying to explain...*
The shaders are the essence of this entire logic. And that's exactly what was implemented in this report.

## 2026-10-11 21:40:38:<br>Category: Development Report<br>Topic: Implement Basic Solver
*Because the "MVP" requirement to submit, this task was pulled from the schedule and granted maximum priority...*
This is the humble beginning of the master piece.
The Basic components are implemented, including:
- Verlet Solver
- Gravity Solver
- Collision Solver
And foundational components such as SoA data structure and Engine Interface are all introduced.
Suffices to say that, this is a functional Particle Physics Engine already.

*It is actually very incredible I can complete this in this few hours. I was expecting a much harder route getting here, given that the first time me making the physics engine took me an entire month to figure out the logic.*
*But it's actually pretty reasonable. I am building this standing on top of the sholder of a giant. Obviously that giant is me, of course.*

### Testing Result
Substep 8:
- 500 object
- 90 average FPS

Substep 1:
- 1500 object
- 120 average FPS

## 2026-10-11 22:52:02:<br>Category: Development Report<br>Topic: Drastical Performance Increase
*Uhhh so I just found out that this entire time I've been benchmarking the program was compiled in debug mode.*
In release build, the performance profile was shocking:

Substep 1:
- 7500 object
- 200 average FPS

## 2026-10-15 17:14:49:<br>Category: Research Report<br>Topic: Using `sol2` to Integrate Lua
For performance's sake, a proper engine is almost always written in a compiling language, such as C/C++.
But the trade off of compiling language is the lack of dynamic testing capability and protability. And that's the entire reason for scripting language to exist.
Integrating with a scriping language, means that the engine expose a set of API in that language, and grant the script capability of altering the engine's behavior.

It allows external, non-compiling scripts being able to program custom logic with the engine, without re-compiling the entire engine.
Which means now once the engine is stable, programmer can use scripting language to utilize the engine, without modifying the source code and recompile the entire thing.

---
Lua is is choosen scripting language of the TXParticleDynamics because how well it could integrate with C++ and how clean it's syntax is. Also for it's absolute performance advantage among other scripting languages.
*The Lua here is refering to standard Lua, excluding LuaJIT or Luau. It is a weighted decision to omit LuaJIT, because it lacks support of modern lua, and it's performance optimization is negligible in the usage of the engine, in which the lua script never perform heavy computation.*

---
*The raw C API of lua is very convoluted to use, therefore a wrapper library in this stage is absolutely necessary.*
`sol2` is the most mature library in the community, heavily utilizing template meta programming to reduce runtime overhead. It is almost the physical limit of the speed for cross boundary lua <-> C++ communication.

### CMake Setup
*Although sol2 is header only and theoretically be used with only one header, proper CMake setup is still strongly encouraged.*
First, clone the [offical repository](https://github.com/ThePhD/sol2).
In CMakeLists.txt:
```cmake
find_package(Lua 5.4 EXACT REQUIRED)
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/sol2)

# add_executable / add_library

target_include_directories(<your-project-name> PRIVATE
	${LUA_INCLUDE_DIR}
)
target_link_libraries(<your-project-name> PRIVATE
	${LUA_LIBRARIES}
	sol2::sol2
)
```
*The `find_package(Lua 5.4 EXACT REQUIRED)` was to resist any newer Lua version that sol2 doesn't support.*
*`<your-project-name>` is the target identifier of the project.*

### Exposing Interface
#### Basic sol2 API
The generic inclusion of sol2 resembles the following:
```cpp
#define SOL_ALL_SAFETIES_ON 1
#include "sol/sol.hpp"
```

To begin, a `sol::state` object must be created. A `sol::state` object represents a running Lua virtual machine. All operations of sol2 involves this state object.
```cpp
sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string); // setup the lua standard library is also necessary
```

To set value of a variable or create a variable:
```cpp
lua["variableName"] = <variable-value>
```
This semantic supports a various of types, in which they will be automatically converted to it's corresponding lua type.
It supports all primitive built-in types, strings, pointers, and functions.
sol2 also provides method for user to define their own types and how those types can be accessed, via `lua.new_usertype`:
```cpp
m_impl->lua.new_usertype<tx::ParticleDynamics::EngineParameters>(
    "EngineParameter",
    "MaxEntityCount", &tx::ParticleDynamics::EngineParameters::MaxEntityCount,
    "SubstepCount", &tx::ParticleDynamics::EngineParameters::SubstepCount)
```

To expose a function, it is exactly the same as if trying to set a variable. Every function can be interpreted as a functor:
```cpp
lua["functionName"] = [](){}; // lambda (recommanded)
lua["functionName"] = std::function<void()>{}; // std::function
lua["functionName"] = &func; // function pointer
```

To bind a lua variable with a reference of a C++ variable, use `std::ref`:
```cpp
lua["ref"] = std::ref(v);
```
This create a "persistent mapping" of that variable in lua, in which it operates exactly like reference in C++, when it's edited on one side it is immediately visible on the other side.

To read a variable, use implicit conversion or use explicit `.get()` function:
```cpp
int v = lua["variableName"];
lua["variableName"].get<int>();
```

To null a variable (set it's value to `nil` and remove it from it's scope):
```cpp
lua["variableName"] = sol::lua_nil;
```

#### Organizing Interface with Table
There's no native namespace in Lua. Organizing free functions and globals in a table is the standard practice.
The syntax of accessing a table is exactly the same as accessing the global state. *It because the entire Lua global scope is intrinsically a giant hash table, no difference then a Lua table instance.*
To create a table:
```cpp
lua["tableName"] = lua.create_table("tableName")
lua.create_named_table("tableName")
```

### Creating API Definition for Linters
Because Lua is dynamically typed, there's no way for the linter to know the API you ingected, therefore you need to explicitly provide it in a separate file.
This file is a "definition file", it don't perform any actual operations, it is only a hint for the linter to know what field are there.
*This is called LuaCATS (Lua Comments And Type System).*
```lua
---@meta
-- The ---@meta declares that this file is a definition file

-- define a class type
---@class MyClass
---@field value number
---@field func fun(val: number)

-- define an instance
---@type MyClass
obj = {}
```

### Executing Lua Script
To directly execute a lua script:
```cpp
lua.script("<script>")
lua.script_file(<file-path>)
```

But notice that calling script function requires the lua parser to parse it into byte code everytime. For scripts being frequently called, explicitly compile the script once and cache the byte code is vastly superior.
```cpp
sol::load_result res = lua.load("script");
sol::protected_function script;

if (!result.valid()) {
	// Error
} else {
	script = res;
}

script();
```

### Compilation Time Concern
The sol2 library is header only and heavily utilize template meta programming, resulting noticable compilation overhead if included naively.
To reduce compilation overhead, sol2 should never be included in a header and propagate to the entire codebase.
Limit sol2 inclusion in cpp files, along with the Pimpl Pattern is the standard practice of sol2 inclusion.

## 2026-10-15 17:16:34:<br>Category: Personal Journal<br>Topic: How I've decided to finally integrate scripting language
For quite a long time, all the way back to my first physics engine, I always desire some kind of external configuration for my engine that can change it's behavior, so I don't need to compile the entire thing everything I want to test features. I always like to tweak numbers over and over again just to see how things interact under different circumstances, and the rate of me doing it is so frequent that the few seconds of C++ compilation is unbearable.
At first I made a Json library, hoping I could put my configuration there and only modify that. It did work, but only for a small portion of the demand. The core problem was Json is limited to static values and cannot express logic, thereby forcing me back to C++ when I want to create different demos.
And I understodd that a Turing Complete language is absolutely necessary to accomplish what I am targeting, so for a long time I was theorycrafting my own scripting language *(What? You think I'm going to trust a random language that I never knew before? What if it's slow? What if it's as horrible as Python?)*, in which I eventually made (Yes I am talking about [TXCSL](https://github.com/TXWD1234/TXCompute/tree/main/Software/components/TXCSL)) and quickly threw away, because I've realized how convoluted making a interpreter is, and how hard it is to make it ***FAST***.
And eventually I discovered Lua. I noticed it when [hyprland](https://github.com/hyprwm/hyprland) decided to do a massive overhaul of the API and completely switch to Lua for configuration, and I had to desperately watch my Arch installation crashing over and over, while I am urgently having a project to finish, thus miserably forced to use Windows for a few weeks and finished the project, only to come back hopelessly look at the Lua API like it's some kind of alien gibberish.
*Okay that was over exaggerating, but you get the idea.*
The time I acknowledge it and really started consider integrating with it was when I noticed that Factorio, the one most performance minded and heavily optimized game I known of all time heavily relies on Lua scripting for all it's content. I was like: "Factorio is trying to be very fast and is indeed very fast, and it uses Lua, so I probably should as well."
Upon further investigation, I realized that the deal Lua had offered was much better then what I thought. The fact that it is designed to be integrated with C/C++ from the beginning made me really set a preference on the language.
So while already looking forward to integrating with Lua, I started this TXParticleDynamics project. I immediately identified the demand of a scripting API when I first completed the engine prototype, and decided this would be the best opportunity to integrate it, in which I did.
Unsurprisingly, Lua had delivered even more surprise the moment I actually get into it. The simplicity and minimalism pursuit of the syntax is one thing that really triggers me; How clean and straight forward using sol2 library to integrate with it is another thing. Also I just love that it produced almost negligible performance overhead despite running 60 times a second.
And yeah, that's my story with Lua, thank you for reading I guess. *This report was started to be a technical progress report of the recent few days, but I've realized it's getting too personal and geniously change the category to personal journal.*

## 2026-10-16 17:45:39:<br>Category: Development Report<br>Topic: Lua Integration and numerous Post Prototype Refinement
*Maybe rather then celebrating when a project reaches a major milestone, I should start crying instead. Because I know the following days would be bloated with extremely boring refinement and stress testing. Or maybe I really should celebrate, because I know that those seemingly trivial refinements are practically the most important development stage of the project, and is what actually makes a project from a prototype to a product.*

After the legendary burst of production, which had established the foundation of TXParticleDynamics: The collision solver, what was immediately followed was maintenance issues and accessibility integrations.
The most major progress made during this update was the introduction of **Lua Integration**. The integration provides a set of APIs exposed to the lua scritping environment and can be called from a script to interfere engine behavior.
During this update, the substep logic was corrected and delta time integration was adjusted to correct state.
Some other moderate or minor changes made during this update:
- Added isometric rendering option
- Added numerous feature interfaces including `applyImpulse`, `applyRotationalForce`, etc.
- Numerous demos in was implemented in form of Lua script packs

## 2026-10-18 12:11:26:<br>Category: Development Report<br>Topic: Infrastructure: Concurrency
Concurrency is one of the most important planned optimization for the engine. Multi-thread execution divides update time almost linearly with the thread count and made exploiting 100% of the CPU's power possible.
In this update, the infrasturcture of concurrency was implemented.
Introducing `TaskThread`, a single thread manager capable of storing assigned tasks and execute them sequencially.
It can accept multiple task functions, although the parameter must be identical, it can be overcomed by making the parameter a union or `std::variant`, or reusing fields of a struct.
It is optimized for performance, *because of course it is,* using no locks but purely atomic operations.

## 2026-10-18 15:55:52:<br>Category: Development Report<br>Topic: TXLib: TXMath: Geometry Refactor
Moving on to proper geometry integration such as Grid System, the project cannot continue to rely on the ancient TXMath from a year ago. Several flaws founded before, along with the inefficiency and inperfection of the entire architecture had driven this refactor, and now it is finally the time to do it.
In this refactor, the entire `vec2` / `Coord` implementation was refactored to the templated `Vec<D, T>` version, the `geom_utils.hpp` utilities are also refactored for correctness and consistency.

## 2026-10-18 15:58:42:<br>Category: Development Documentation<br>Topic: Grid System Overview
The objective of week 1 (next week) is to implement the Grid System Optimization.
Grid System Optimization turns the update time scaling from exponential to almost linear, which drastically decreases the total update time.
It works by dividing the entire simulation world space into small uniform sized grids. Then during collision update, the objects in each grid would only need to compare against the objects in the surrounding grids, instead of every other object in the entire simulation world.

### Constructing and Updating the Grid System Array
The Grid System Array (gsArray) an array storing IDs of objects in each grid. The gsArray is arranged in the form of PackedPartedArray, mapped by Grid System Index Array (gsIndexArray).

#### Construct the GSArray
The construction of the GSArray utilizes algorithm of Radix Sort paired with Counting Algorithm.
There are 4 stages of the construction of the array, listed as following:
- Find grid pos of each object (gridpos array)
- Construct gsIndexArray
  Independent from the following stages, the construction of gsIndexArray first need to read the gridpos array and count the occurance of each grid index in another N array, and the turn that array into a PSA array as the final index array. In which the first step can only be done in a single thread.
- Primary sort
  Sorting gridpos (Lower 10 bit) into primary bucket array, which stores the upper 10 bits of the gridpos.
  Along with that, an index array aligned with the buckets is also constructed, holding information of the object ID of each corresponding gridpos in the primary bucket array.
- Secondary sort
  Using primary bucket array, the secondary sort create the final GSArray by sorting each element in the primary bucket array, and instead of putting the gridpos value to the destination bucket slot, the corresponding object ID of that element is placed in the slot.

#### Update the GSArray
Updating the array is just reconstructing the array, since given that the position distribution of the objects are complete unpredictable with actual calculation, any extra branching or policy to attempt reuse the previous gsArray is completely extra overhead.




## 2026-10-19 10:19:37:<br>Category: Development Documentation<br>Topic: Grid System Infrastructure
- TaskThread for multi-threading
  - SPSCQueue
- ZGrid
- PackedPartedView