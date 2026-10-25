# Architecture Decision Records (ADRs)

**Table of content:**  
[Overview](#overview)  
[ADR Index](#adr-index)  
┕ [Source Tree & Build Architecture](#source-tree--build-architecture)  
┕ [Module Anatomy & Subsystem Patterns](#module-anatomy--subsystem-patterns)  
┕ [Core Systems & Concurrency Primitives](#core-systems--concurrency-primitives)  
┕ [Math & Core Types](#math--core-types)  
┕ [Platform & OS Abstraction](#platform--os-abstraction)  
┕ [Concurrency & Event Routing](#concurrency--event-routing)  
┕ [Window Management](#window-management)  
┕ [Input System Architecture](#input-system-architecture)  

## Overview

The **Architectural Decision Records (ADRs)** directory documents the foundational engineering
choices that shape the `gp-engine`.

Because `gp-engine` is built around a Deconstructionist Pedagogy, the why is just as critical as
the how. These records serve as a permanent, educational ledger of the trade-offs, constraints,
and AAA standards that informed our architecture. They explain why we enforce strict physical
module boundaries, why we reject the STL in favor of custom memory-safe structures, and how we
progressively abstract complex OS and hardware realities without hiding them behind a black box.

Each ADR acts as an immutable contract for the codebase. If you are learning the engine,
contributing a new module, or simply studying modern engine architecture, this index is your map to
the engine's DNA.

## ADR Index

### Source Tree & Build Architecture

These records define the physical layout of the repository, the declarative build system, and the
strict visibility rules that prevent the codebase from becoming a tangled "ball of mud."

- [**ADR-0001**: Adoption of Public/Private/Internal Directory Discipline for Modules](./ADR-0001.md)
  > Establishes a strict three-tier directory structure (`public/`, `private/`, `internal/`)
  > mechanically enforced by the build system to guarantee module encapsulation and prevent
  > uncontrolled dependencies.
- [**ADR-0003**: CMake as Build Generator with GPBT as Declarative Orchestration Layer](./ADR-0003.md)
  > Adopts CMake as the foundational build system paired with a custom declarative orchestration
  > layer (GPBT) to enable discovery-order independence and mechanical enforcement of module
  > visibility.
- [**ADR-0004**: Lifecycle Separation via Top-Level Source Directories](./ADR-0004.md)
  > Segregates the `source/` tree into distinct top-level directories (`runtime/`, `developer/`,
  > `launch/`, etc.) that strictly encode build-lifecycle rules, ensuring tooling and editor code
  > never leak into shipping game binaries.
- [**ADR-0005**: Monorepo Layout and Repository Root Conventions](./ADR-0005.md)
  > Defines a strict five-folder monorepo layout and establishes `lower-kebab-case` directory naming
  > conventions to maintain a clean repository root and reduce onboarding friction.
- [**ADR-0008**: GPBT Extracted to an Independent, Versioned Repository](./ADR-0008.md)
  > Extracts the custom GPBT build tool into a standalone, version-controlled repository linked as a
  > Git submodule, decoupling its release cadence from the main engine.

### Module Anatomy & Subsystem Patterns

These records dictate how pluralistic systems (like rendering, audio, and physics) are designed to
support multiple APIs while maintaining a single, unified interface for the engine.

- [**ADR-0002**: Abstracted Rendering Hardware Interface (RHI) with Runtime-Loaded Backends](./ADR-0002.md)
  > Mandates a `base/` plus N-backends decomposition for the Rendering Hardware Interface, ensuring
  > concrete graphics APIs (Vulkan, D3D12, Metal) are loaded as dynamic plugins at runtime to
  > eliminate compile-time coupling.
- [**ADR-0006**: Co-Located Documentation, Tests, and Benchmarks per Module](./ADR-0006.md)
  > Rejects satellite test directories in favor of co-locating tests, benchmarks, and documentation
  > directly inside each module to ensure these assets scale and refactor atomically with the code
  > they describe.
- [**ADR-0007**: Uniform Base+N-Backends Pattern for All Pluralistic Subsystems](./ADR-0007.md)
  > Generalizes the abstract RHI structure to all pluralistic subsystems (Audio, Physics, Parsers),
  > requiring a central `base/` module alongside independent, dynamically loaded backend modules.

### Core Systems & Concurrency Primitives

These records detail the foundational multi-threading primitives and core data structures of the
engine. By rejecting the C++ Standard Library (`<thread>`, `<atomic>`) in favor of custom,
STL-free implementations, the engine guarantees ABI stability across dynamic modules, eliminates
hidden heap allocations, and provides the absolute memory control required for high-performance,
lock-free architectures.

- [**ADR-0021**: STL-Free Concurrency Primitives and Platform Abstraction](./ADR-0021.md)
  > Rejects `<thread>`, `<mutex>`, and `<atomic>` in favor of custom engine-native primitives
  > (`gp::thread::Atomic`, `gp::thread::Thread`, `gp::container::SPSCByteRingBuffer`) placed in the
  > core module, ensuring strict memory control and ABI stability across module boundaries.
- [**ADR-0022**: Functional, Stateless Thread Execution over Object-Oriented Payloads](./ADR-0022.md)
  > Rejects object-oriented thread interfaces (e.g., `IRunnable`) and `std::thread` in favor of
  > stateless, C-style function pointers with `void*` context payloads, eliminating virtual dispatch
  > overhead and forced heap allocations to perfectly mirror low-level OS APIs and support
  > data-oriented Job Systems.
- [**ADR-0023**: Zero-Allocation Thread Handoff via Suspended Creation and OS Synchronization](./ADR-0023.md)
  > To prevent heap allocations and dangling pointers during OS thread creation, the engine mandates
  > a synchronized handoff protocol where threads are spawned in a suspended state
  > (`CREATE_SUSPENDED`). Startup parameters (including string names and context pointers) are passed
  > via a parent-stack-allocated payload, and the parent thread blocks on an OS event until the child
  > securely copies the data to its own local stack.
- [**ADR-0024**: Strict Compile-Time Lock-Free Guarantees via Concept-Constrained Atomics](./ADR-0024.md)
  > The C++ standard library (`std::atomic<T>`) silently falls back to hidden OS software locks if
  > `T` exceeds the hardware's native register size, silently destroying real-time thread scaling.
  > The engine strictly forbids this by constraining `gp::Atomic<T>` with a `C++20` concept
  > (`AtomicValue`). This guarantees that any atomic instantiation is strictly 1, 2, 4, or 8 bytes
  > and trivially copyable, ensuring it always compiles 1:1 down to hardware-native lock-free
  > instructions (e.g., `LOCK CMPXCHG`) or fails at compile-time.
- [**ADR-0025**: MESI Cache-Line Isolation and Shadow Indexing in Lock-Free Structures](./ADR-0025.md)
  > To prevent "False Sharing" (cache-line ping-ponging) from bottlenecking cross-core throughput,
  > all lock-free communication primitives (e.g., `SPSCRingBuffer`) must explicitly align and pad
  > their internal state to hardware cache-line boundaries (64 bytes), separating Read-Only,
  > Producer-Modified, and Consumer-Modified zones. Furthermore, cross-thread atomic reads must be
  > minimized using local "Shadow Index Caching," restricting interconnect bus traffic to only when
  > a buffer is completely full or completely empty.
- [**ADR-0030**: 64-Bit Integer Nanoseconds and Rational Frame Rates as Canonical Time Units](./ADR-0030.md)
  > Storing accumulated engine time in 32-bit `float` degrades precision past 1 millisecond after
  > just 4.5 hours of uptime, while storing frame rates as floating-point seconds
  > (`1.0f / 60.0f = 0.016666668f`) introduces cumulative rounding drift during long cutscenes and
  > deterministic replays. The engine mandates 64-bit integer nanoseconds (`gp::Int64` for
  > `Duration`, `gp::UInt64` for `Timestamp`) as the canonical internal representation ($\pm 292$
  > years range at $1\text{ ns}$ resolution) alongside rational `FrameRate`
  > (`numerator` / `denominator`) for exact frame intervals. Floating-point deltaTime
  > (`Float32` / `Float64`) is strictly a read-only leaf view generated for shader and physics
  > integrators.
- [**ADR-0031**: Dual Hardware Counters and Per-Thread High-Resolution Kernel Waitable Timers](./ADR-0031.md)
  > Calling Win32 `Sleep(1)` defaults to a 15.625ms OS scheduler quantum, while globally forcing
  > `timeBeginPeriod(1)` degrades system-wide power efficiency and battery life across all running
  > processes. Furthermore, OS monotonic clocks (`QueryPerformanceCounter`) have higher read latency
  > (~15–25ns) than raw CPU cycle counters (`__rdtsc` ~1–3ns). `gp::platform::Time` abstracts both
  > an invariant monotonic nanosecond clock (with a 1-cycle multiply fast-path when QPC runs at
  > 10 MHz) and a raw CPU cycle counter (`cpuCycles()`) for micro-profiling, while implementing
  > sub-millisecond thread sleep via thread-local `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` kernel
  > objects on Windows and `clock_nanosleep` on POSIX.
- [**ADR-0032**: Hierarchical Simulation Clock Domains with Multiplicative Time Dilation](./ADR-0032.md)
  > A single global `DeltaTime` cannot simultaneously support pausing the game world while keeping
  > UI animations smooth, triggering a 0.05x hit-stop on characters while keeping camera shake at
  > 1.0x, or running network prediction at an independent rate. Inspired by Frostbite and Decima, the
  > engine separates hardware timekeeping (`EngineClock`) from hierarchical domain clocks
  > (`SimulationClock`). Any `SimulationClock` may attach to a parent clock; pause states propagate
  > downward (`isEffectivelyPaused()`) and time dilation scales multiply (`getEffectiveTimeScale()`),
  > with zero heap allocations.
- [**ADR-0033**: Configurable Outlier-Rejected Delta Smoothing and Fixed-Accumulator Interpolation ($\alpha$)](./ADR-0033.md)
  > Raw frame-to-frame deltas suffer from micro-jitter caused by OS background threads and driver
  > buffer queues, while physics simulations explode if fed variable deltas or unbounded sub-steps
  > during a CPU stall ("Spiral of Death"). The clock subsystem provides selectable delta
  > smoothing (Raw, Exponential Moving Average, or Unreal-style Outlier-Rejected Rolling Window)
  > alongside a deterministic Fixed-Timestep Accumulator that outputs discrete step counts capped by
  > `maxSubSteps` and a normalized interpolation factor $\alpha \in [0.0, 1.0)$ for smooth visual
  > rendering between fixed physics ticks.

### Math & Core Types

These records cover the mathematical and foundational data structure decisions that ensure
engine-wide consistency.

- [**ADR-0009**: Vector4 default construction uses homogeneous point coordinates](./ADR-0009.md)
  > Establishes that the default constructor for the engine's 4D vector type initializes as a
  > homogeneous point (w=1) rather than a direction (w=0) or a zero-vector, ensuring correct affine
  > transformations by default.

### Platform & OS Abstraction

These records outline how the engine isolates operating system complexities using an injected
Hardware Abstraction Layer (HAL).

- [**ADR-0010**: Adoption of SDL3 as the Primary Platform Abstraction Backend](./ADR-0010.md)
  > Defines the choice of SDL3 to handle the chaotic realities of OS windowing, input polling, and
  > device discovery, acting as the concrete implementation behind the engine’s generic HAL.
- [**ADR-0011**: Dependency-Injected Application Backend Interface for Platform Agnosticism](./ADR-0011.md)
  > Establishes the `IApplicationBackend` pattern, ensuring the engine logic remains completely blind
  > to the underlying OS/SDL3 layer, with the specific backend injected at the executable level
  > (e.g., inside `gp_editor`).
- [**ADR-0012**: Centralized Application Layer for Platform Lifecycle and Event Orchestration](./ADR-0012.md)
  > Defines the `gp::application::Application` class as the central manager that owns the injected
  > backend, tracks active windows, and acts as the orchestrator between OS events and engine
  > subsystems.
- [**ADR-0028**: Zero-Allocation Cached Display Topology and Shadow Mode Tables](./ADR-0028.md)
  > Querying OS display managers (`EnumDisplaySettings`, `SDL_GetFullscreenDisplayModes`) at runtime
  > triggers synchronous EDID/driver calls and heap allocations that stall the calling thread. To
  > eliminate runtime allocations and OS stalls, `gp::hal::generic::Display` caches monitor bounds,
  > work areas, HDR luminance metadata, and deduplicated `DisplayMode` tables inside fixed-capacity
  > inline storage (`kMaxCachedModes = 64`). The topology is populated once on startup and
  > incrementally synchronized via `DisplayEvent` packets from the lock-free `SPSCByteRingBuffer`.
- [**ADR-0029**: Centralized Display Topology, Spatial Resolution, and Headless Virtual Display Fallback](./ADR-0029.md)
  > Scattering display enumeration, multi-monitor spatial math, and hotplug table management across
  > individual OS backends duplicates complex geometry logic and causes nullptr crashes when the
  > engine runs on headless CI runners or dedicated servers without a physical display. To guarantee
  > a uniform contract, `gp::hal::generic::ApplicationBackend` centralizes the active display
  > pointer table (`kMaxDisplays = 16`), implements deterministic spatial resolution
  > (`getDisplayForPoint` via nearest clamped boundary distance and `getDisplayForRect` via maximum
  > overlapping pixel area), and automatically provisions a synthetic `1920x1080 @ 60Hz` virtual
  > primary display whenever zero physical monitors are detected.

### Concurrency & Event Routing

These records define how data moves safely between threads, prioritizing determinism and
cache-locality.

- [**ADR-0013**: Multi-Threaded Event Architecture via Lock-Free Queues and Immutable Frame State](./ADR-0013.md)
  > Mandates that OS events are pumped by a dedicated OS thread into a Single-Producer/Single-Consumer
  > lock-free ring buffer, allowing the Game Thread to drain them at the start of a frame to guarantee
  > a deterministic, lock-free, immutable input state.
- [**ADR-0019**: Type-Length-Value (TLV) Byte-Stream for Cross-Thread Event Routing](./ADR-0019.md)
  > Rejects fixed-size Tagged Unions and polymorphic event objects in favor of a contiguous raw byte
  > stream (`EventHeader` + payload). This eliminates the "Largest Payload Problem," natively
  > supports variable-length data (like file drop paths or UTF-8 text input), and maximizes CPU
  > cache efficiency.
- [**ADR-0020**: Strict Data-Oriented Design (PODs) at the Hardware-to-Engine Boundary](./ADR-0020.md)
  > Mandates that all events crossing from the OS thread to the Game thread must be Trivially
  > Copyable, Standard Layout Plain Old Data (POD) structs. Explicitly forbids the use of C++
  > inheritance, virtual tables, RTTI, and heap allocations (`new`/`delete`) at the HAL level to
  > prevent heap fragmentation and lock contention.
- [**ADR-0026**: Two-Tier Backpressure and Critical Reserve Admission in Lock-Free Event Queues](./ADR-0026.md)
  > High-polling-rate peripherals (8000 Hz mice, controller IMUs) can saturate a fixed-capacity
  > lock-free ring buffer during temporary Game Thread stalls, causing critical discrete state
  > transitions (such as `KeyState::Released` or `WindowAction::CloseRequested`) to be dropped and
  > leaving the engine in a desynchronized "stuck key" state. To prevent this while maintaining a
  > non-blocking OS message pump, the event producer enforces a two-tier admission policy:
  > high-frequency continuous events (`Mouse::Moved`, `Gamepad::AxisMoved`, `Sensor`,
  > `Touch::Motion`) are shed once free ring buffer capacity drops below a reserved threshold
  > (`criticalReserveBytes`), preserving dedicated headroom strictly for discrete lifecycle and
  > button-state transitions.
- [**ADR-0027**: Event-Driven Window Shadow State for Lock-Free Game Thread Queries](./ADR-0027.md)
  > Native OS windowing APIs (Win32, Cocoa, Wayland, SDL3) restrict window property queries
  > (`GetWindowRect`, `SDL_GetWindowSize`) to the main OS thread; invoking them from the Game or
  > Render threads causes race conditions, kernel syscall overhead, and mid-frame state tearing. To
  > guarantee thread safety and zero-cost queries, `gp::hal::generic::Window` maintains a local
  > "Shadow State" (cached dimensions, framebuffer pixel extent, position, DPI scale, focus, and
  > visibility flags). This shadow state is mutated exclusively on the Game Thread when draining
  > `WindowEvent` packets from the lock-free `SPSCByteRingBuffer`, turning all IWindow getters into
  > deterministic, `constexpr` L1-cache reads.

### Window Management

These records establish type-safe patterns for dealing with native OS windows and screen coordinates.

- [**ADR-0014**: Type-Safe Native Handles Using Tagged Unions over Opaque Void Pointers](./ADR-0014.md)
  > Rejects the use of a generic `void*` for OS window handles (HWND, Wayland surface, etc.) in favor
  > of a tagged union (Struct + Enum) to enforce type-safety and exact backend identification at
  > compile/runtime.
- [**ADR-0015**: Strict Semantic Differentiation Between Point2D (Position) and Extent2D (Size)](./ADR-0015.md)
  > Establishes separate, type-safe POD structs for coordinate positions (`Point2D`) versus
  > dimensional sizes (`Extent2D`) in window management, preventing logical bugs caused by passing a
  > width/height into an x/y parameter.

### Input System Architecture

These records detail how raw hardware signals are translated into ergonomic, domain-specific state
for gameplay programmers.

- [**ADR-0016**: Explicit Separation of Physical ScanCodes and Logical KeyCodes](./ADR-0016.md)
  > Separates `ScanCode` (the physical hardware location of the key on the board, used for gameplay
  > controls like WASD) from `KeyCode` (the localized OS character mapping, used for UI text typing).
- [**ADR-0017**: Domain-Specific Input State Abstractions for Keyboard, Mouse, and Gamepad](./ADR-0017.md)
  > Defines localized, independent classes (`Keyboard`, `Mouse`, `Gamepad`) that maintain their own
  > previous and current state arrays, providing ergonomic polling APIs (e.g., `isKeyPressed`) for
  > gameplay programmers.
- [**ADR-0018**: Capability-Based Device Abstraction (Input, Haptic, Spatial)](./ADR-0018.md)
  > Establishes a modular hierarchy for hardware devices, separating them by capability
  > (e.g., `InputDevice` for buttons, `HapticDevice` for rumble/force-feedback, `SpatialDevice` for
  > VR/Gyro tracking) rather than monolithically grouping them.
