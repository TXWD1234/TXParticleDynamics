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