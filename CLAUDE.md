<!-- ============================================================
     STOP! READ THESE RULES BEFORE DOING ANYTHING
     ============================================================ -->

> **⛔ MANDATORY RULES - VIOLATIONS BREAK THE PROJECT ⛔**
>
> 1. **NEVER use `2>nul` or `> nul`** - Creates undeletable "nul" file on Windows
> 2. **NEVER use Unix commands** - No `mkdir -p`, `rm -rf`, `cat`, `grep`, etc.
> 3. **Use `node` directly for TypeScript** - NOT tsx, ts-node, or tsc
> 4. **DISCUSS before implementing** - Answer questions first, code after agreement

---

# V8-JSI Project Guide

## Quick Reference: Scripts Development

**Working directory for TypeScript/CLI scripts: `scripts/`**

All script development (sync.ts, ai-merge.ts, modules) uses the `scripts/` folder which has its own `package.json` and `tsconfig.json`.

**Claude Code's Bash tool requires wrapping PowerShell commands:**

```bash
# Common commands (always use this pattern):
powershell -Command "cd 'e:\GitHub\microsoft\v8-jsi\scripts'; npm test"
powershell -Command "cd 'e:\GitHub\microsoft\v8-jsi\scripts'; npm run typecheck"
powershell -Command "cd 'e:\GitHub\microsoft\v8-jsi\scripts'; npm run fix"

# Run a script:
powershell -Command "cd 'e:\GitHub\microsoft\v8-jsi\scripts'; node sync.ts --dep nodejs --help"
```

| Command | Purpose |
|---------|---------|
| `npm test` | Run all tests |
| `npm run typecheck` | Type check all TypeScript |
| `npm run fix` | Fix linting AND formatting |
| `npm run lint` | Check linting only (ESLint) |
| `npm run format` | Check formatting only (Prettier) |

For detailed script development docs, see [scripts/CLAUDE.md](scripts/CLAUDE.md).

## Important: Discuss Before Implementing

**When the user asks a question, ANSWER the question and DISCUSS solutions with them.** Do not immediately jump to writing code or creating files. Have a conversation first to understand what approach they prefer, then implement only after getting agreement.

## CRITICAL: Windows-Only Commands

**This is a Windows-only project. NEVER use Unix-style shell commands.**

### The "nul" File Problem

On Windows, `nul` is a reserved device name (like `/dev/null` on Unix). If you accidentally create a file named `nul`, it becomes nearly impossible to delete. **DO NOT create this file.**

**NEVER DO THIS:**
```bash
# WRONG - creates a "nul" file on Windows!
command 2>nul
command 2>/dev/null
some_command > nul
```

**CORRECT alternatives:**
```bash
# Redirect to $null in PowerShell
powershell -Command "command 2>`$null"

# Or just let errors show (often fine)
command

# Or use 2>&1 to merge streams
command 2>&1
```

### Other Forbidden Commands

- **DO NOT** use `mkdir -p` (use `New-Item -ItemType Directory -Force`)
- **DO NOT** use `rm -rf` (use `Remove-Item -Recurse -Force`)
- **DO NOT** use `cat`, `grep`, `sed`, `awk` (use PowerShell equivalents or the Grep/Read tools)
- **DO NOT** use Unix path separators in shell commands (`/path/to` - use `\path\to` or let Node.js handle paths)

### Safe Commands

- Use `dir` instead of `ls`
- Use `type` instead of `cat` (or better: use the Read tool)
- Use PowerShell cmdlets: `Get-Content`, `Set-Content`, `New-Item`, `Remove-Item`
- Let Node.js/TypeScript handle file operations when possible

### Running Commands

**Claude Code's Bash tool runs bash, even on Windows.** To execute commands properly, wrap them in `powershell -Command`:

```bash
# CORRECT - wrap in PowerShell with single-quoted paths:
powershell -Command "cd 'e:\GitHub\microsoft\v8-jsi\scripts'; npm test"
powershell -Command "& 'e:\GitHub\microsoft\v8-jsi\deps\nodejs\out\Release\v8jsi_test.exe'"

# WRONG - raw bash doesn't understand Windows paths:
cd e:\path; command                    # FAILS
cd /d "e:\path" && command             # FAILS - /d is cmd.exe, && is bash
```

**Key pattern:** `powershell -Command "cd 'FULL_PATH'; command"`

## Project Overview
V8-JSI is a JavaScript Interface (JSI) implementation using the V8 JavaScript engine. It provides a bridge between native code and JavaScript, primarily used in React Native applications.

## Build System

### Node.js-based Build (Current Development)
The project is transitioning to use V8 from Node.js instead of standalone Google V8.

**Build location**: `deps/nodejs/`

**Canonical entry point** — use `scripts/build.ts` (passes the right narrow-build flags automatically):
```powershell
powershell -Command "Set-Location 'e:\GitHub\microsoft\v8-jsi\scripts'; node build.ts --platform x64"
```

**Release build** (raw `vcbuild.bat`, with narrow-build flags):
```powershell
powershell -Command "Set-Location 'e:\GitHub\microsoft\v8-jsi\deps\nodejs'; .\vcbuild.bat release v8jsi without-intl no-node no-cctest no-embedtest no-openssl-cli no-fuzzers no-nop no-overlapped-checker"
```

**Debug build** (raw `vcbuild.bat`, with narrow-build flags):
```powershell
powershell -Command "Set-Location 'e:\GitHub\microsoft\v8-jsi\deps\nodejs'; .\vcbuild.bat debug v8jsi without-intl no-node no-cctest no-embedtest no-openssl-cli no-fuzzers no-nop no-overlapped-checker"
```

**Output**:
- `deps/nodejs/out/Release/v8jsi.dll` - Main shared library
- `deps/nodejs/out/Release/v8jsi_test.exe` - Test executable
- `deps/nodejs/out/Debug/v8jsi.dll` / `v8jsi_test.exe` for Debug builds.

Note: `vcbuild.bat` supports an **additive target-list mode** — passing any `no-*` flag switches MSBuild from the solution-level `Build` meta-target to an explicit list of user-facing targets, skipping whichever ones the caller opted out of. The seven flags above opt out of every Node.js binary v8-jsi doesn't ship (node.exe, cctest, embedtest, openssl-cli, fuzzers, nop, overlapped-checker). Running raw `vcbuild.bat release v8jsi without-intl` (no `no-*` flags) still falls through to the legacy whole-solution `Build` target. See [design-part-a.md](e:/GitHub/vmoroz/kb/v8-jsi/new-v8jsi-dll-phase1-cleanup/design-part-a.md) for the full mechanism.

**Clean build** (if you hit stale artifact issues):
```powershell
powershell -Command "Set-Location 'e:\GitHub\microsoft\v8-jsi\deps\nodejs'; .\vcbuild.bat clean"
powershell -Command "Set-Location 'e:\GitHub\microsoft\v8-jsi\scripts'; node build.ts --platform x64"
```

**Key build files**:
- `src/v8jsi.gyp` - Main GYP build configuration for v8jsi
- `deps/nodejs/node.gyp` - Node.js build config (includes v8jsi.gyp when build_v8jsi=1)
- `deps/nodejs/vcbuild.bat` - Windows build script (use `v8jsi` option)
- `deps/nodejs/configure.py` - Configure script (--build-v8jsi flag)

### Build Variables
- `v8jsi_root` = `../..` (relative path from deps/nodejs to v8-jsi root)
- `v8jsi_enable_inspector` = 1 (always-on; v8-jsi must ship with inspector enabled)
- `v8jsi_enable_node_api` = always-on in `v8jsi.gyp` (defines `V8JSI_ENABLE_NODE_API`)
- `v8jsi_test_hooks` = 1 (default). When 1, `v8jsi.dll` exports test-only hooks
  guarded by `#ifdef JSI_TESTING_ONLY` (currently `v8_jsi_test_post_foreground_task`).
  CI/release builds may flip to 0; if so, the `v8jsi_test` target will fail to link
  any test that references a gated symbol.

## Source Code Structure

### Core Sources (`src/`)
- `V8JsiRuntime.cpp` / `V8JsiRuntime_impl.h` - Main JSI runtime implementation (legacy, uses C++ exceptions)
- `v8_core.h` / `v8_core.cpp` - Shared V8 infrastructure (platform, isolate data, TryCatch wrapper)
- `IsolateData.h` - V8 isolate data management
- `V8Instrumentation.cpp/.h` - Performance instrumentation
- `MurmurHash.cpp/.h` - Hash implementation
- `v8jsi.cpp` - DLL exports and test functions

### JSI Interface (`src/jsi/`)
- `jsi.h` / `jsi.cpp` - Core JSI interface
- `jsi-inl.h` - Inline implementations
- `decorator.h` - JSI decorators
- `instrumentation.h` - JSI instrumentation interface

### ABI Layer (`src/jsi_abi/`)
- `jsi_abi.h` - ABI-stable C interface header (shared with hermes-windows)
- `jsi_abi_helpers.h` - C++ helpers for OrError encoding/decoding (shared with hermes-windows)
- `jsi_abi_v8.cpp` - V8 implementation of the ABI (exception-free)
- `JsiAbiRuntime.h/.cpp` - C++ wrapper implementing jsi::Runtime on top of ABI (shared with hermes-windows, uses exceptions for JSI compatibility)

### Public Headers (`src/public/`)
- `V8JsiRuntime.h` - Public runtime API
- `ScriptStore.h` - Script storage interface

### Platform-Specific
- `src/jsi/jsilib-windows.cpp` - Windows JSI library
- `src/etw/` - Event Tracing for Windows

### Inspector (Currently Disabled)
- `src/inspector/` - V8 inspector integration (needs asio.hpp, requires modernization)

## ABI Layer Architecture

The ABI layer provides a stable C interface for JSI, enabling binary compatibility across different compiler versions.

### Key Components
- **jsi_runtime_vtable**: Main vtable with all runtime operations
- **jsi_instrumentation_vtable**: Optional interface for heap profiling (obtained via QueryInterface)
- **jsi_host_object_vtable**: Callbacks for HostObject property access
- **jsi_host_function_callback**: Callback type for native functions

### Exception-Free Implementation
The `jsi_abi_v8.cpp` implementation does not use C++ exceptions, matching V8/Node.js coding style:
- Uses `TryCatch` wrapper (from `v8_core.h`) to capture JavaScript exceptions
- Uses V8's `Maybe`/`MaybeLocal` pattern with `ToLocal()` checks
- Returns `jsi_pending_exception` status for JS errors
- Returns `jsi_generic_failure` with error message for native errors
- Can be compiled with `-fno-exceptions` (GCC/Clang) or `/EHs-c-` (MSVC)

### QueryInterface Pattern
The ABI uses a COM-like QueryInterface pattern for optional interfaces:
```c
jsi_interface_id iid = JSI_IID_INSTRUMENTATION;
const void* vtable;
void* instance;
vtable->query_interface(runtime, &iid, &vtable, &instance);
```

This allows adding new interfaces without modifying the core vtable.

### Well-known Interface IDs
- `JSI_IID_INSTRUMENTATION` - Heap profiling and GC statistics

## V8 API Compatibility Notes

When updating for newer V8 versions, watch for these API changes:
- `Utf8Length()` -> `Utf8LengthV2()`
- `WriteUtf8()` -> `WriteUtf8V2()`
- `Write()` -> `WriteV2()`
- `GetPrototype()` -> `GetPrototypeV2()`
- `SetPrototype()` -> `SetPrototypeV2()`
- `ScriptOrigin` constructor no longer takes Isolate* as first parameter
- `promise->GetIsolate()` removed - use `v8::Isolate::GetCurrent()`
- Property interceptor handlers now return `v8::Intercepted` enum

## Testing

**Test executable**: `deps/nodejs/out/Release/v8jsi_test.exe`

**Run all tests** (Windows command prompt):
```batch
cd deps/nodejs/out/Release
v8jsi_test.exe
```

**Run all tests** (PowerShell or Claude Code):
```powershell
powershell -Command "& 'e:\GitHub\microsoft\v8-jsi\deps\nodejs\out\Release\v8jsi_test.exe'"
```

**Run direct V8JsiRuntime tests only** (exclude ABI runtime):
```batch
v8jsi_test.exe --gtest_filter=*0
```

**Run ABI runtime tests only**:
```batch
v8jsi_test.exe --gtest_filter=*1
```

**Test sources**:
- `src/jsitests_main.cpp` - Test main with V8JsiRuntime factory
- `src/jsi/test/testlib.cpp` - Core JSI test library (from Meta)
- `src/jsi/test/testlib_ext.cpp` - Extended JSI tests (V8-specific)

**Current test status**: 117 tests pass, 0 skipped
- Direct V8JsiRuntime tests (`/0`): All passing
- ABI runtime tests (`/1`): All passing
- Full JSI API support including HostObject, Microtasks, Instrumentation, RuntimeDecorator

## Known Issues

### Inspector Disabled
Inspector code in `src/inspector/` needs `asio.hpp` and modernization for new Node.js APIs. Set `v8jsi_enable_inspector` to 0.

### Minor Compiler Warnings
- `IsolateData.h`: Field initialization order warning (cosmetic)
- `jsi_abi_v8.cpp`: Unused function warning for `setNativeException` (cosmetic, function is used)

## Preprocessor Defines
- `V8JSI_EXPORT` - DLL export macro
- `V8JSI_IMPORT` - DLL import macro
- `V8JSI_ENABLE_INSPECTOR` - Enable inspector support (currently 0)
- `V8JSI_ENABLE_NODE_API` - Enable Node-API integration

## Dependencies
- V8 (from Node.js deps/v8)
- v8_libplatform
- ICU (internationalization, ~30MB impact on binary size)
- OpenSSL, libuv, etc. (from Node.js)

## Binary Size Notes
- Full build with ICU: ~67MB
- Without ICU (--without-intl): ~33MB
- Original v8jsi standalone: ~20MB

## Scripts and Tools

For script development (sync tool, AI merge, type checking), see [scripts/CLAUDE.md](scripts/CLAUDE.md).
