# Zygisk Next Next (for vibe coding)

This document provides strict guidelines, context, and constraints for AI agents working on the ZygiskNextNext repository.

## CORE IDENTITY AND CONSTRAINTS

Before writing, refactoring, or modifying any code, you must internalize the following absolute truths about this project. Violating these is considered a critical failure.

1. IT IS a standalone, from-scratch ZygiskNext (ZN) Module Loader. Its sole purpose is to provide a runtime environment and loading mechanism specifically for modules built against the ZygiskNext API.
2. IT IS NOT ZygiskNext (ZN) itself. Never refer to this project as "ZygiskNext" or claim it is the original ZN project.
3. IT IS NOT a generic Zygisk module loader. The Zygisk API and ZygiskNext API are distinct. Do not conflate them, and do not add generic Zygisk loading logic.
4. IT IS NOT a full Zygisk implementation. Users requiring Zygisk must enable a separate, dedicated Zygisk implementation elsewhere.

## ZN CLOSED-SOURCE AND STRICT CLEANROOM MANDATE

ZygiskNext (ZN) has been closed-source since version v4-0.9.2. The ZN developers enforce a strict copyright notice prohibiting modifications, redistribution, picking (extracting code snippets/binaries), and claims of succession. 

To comply with this and maintain legal and technical independence, you must adhere to the following Cleanroom Design principles:

1. NO COPYING OR PICKING: Under no circumstances should you copy, paste, extract, or directly translate code, functions, or binaries from the original ZygiskNext repository or its released binaries.
2. REVERSE ENGINEERING + CLEANROOM ONLY: If understanding ZN's behavior is absolutely necessary to ensure API compatibility, you may use reverse engineering to observe its external behavior or API contract. The actual implementation in this repository must be written 100% from scratch (Cleanroom) based on those observations, and must not completely copy or blindly replicate ZN's internal source logic.
3. INDEPENDENT SOLUTIONS: If a bug, crash, or missing feature is identified in ZN, do not simply mirror or port ZN's fix. You must analyze the root cause and design an independent, original solution from first principles.
4. API SURFACE ONLY: Rely only on the publicly documented ZygiskNext Module API specifications for compatibility. Internal implementation details (memory management, injection flow, hook chaining) must remain 100% original and distinct from ZN (unless a specific functionality can strictly only be implemented in that exact manner).

## ARCHITECTURE OVERVIEW

- loader/: The core native component (C++), injected into target processes; it loads modules and manages hooks using Dobby (inline hooking), shadowhook (arm/arm64), rv64hook (riscv64), and LSPlt / ByteHook / xHook (PLT hooking). Its sources are split by area under loader/src/: hook/ (engine selection, inline and PLT dispatch, one file per backend in hook/backend/), api/ (ZygiskNextAPI implementation and the per-version tables), hyos/ (HyperOS Runtime), ipc/ (injector daemon protocol), companion/ (companion process), module/ (zn_modules.txt matching and loading), config/, process/ and utils/ (ELF/maps helpers, memfd dlopen). Only entry.cpp exports anything (`znn_loader_init`).
- injector/: The standalone injector executable shipped as bin/<abi>/injector. NOT part of loader/ and never built by it; it is a Rust crate (cargo) with its own Gradle module, which drives cargo for every shipped ABI. Every `unsafe` block lives under injector/src/sys/, and the crate root denies `unsafe_code` so that stays enforceable.
- loader/src/include/: The ZygiskNext API header (zygisk_next_api.h) a module is built against; the ELF symbol resolver and /proc maps helpers live in loader/src/utils/.
- external/: (repository root) Third-party sources used by more than one native module (currently the LZMA SDK). Dependencies of a single module stay in that module's src/external/.
- module/: The Magisk / KernelSU / APatch module wrapper. Handles installation, environment setup, permission management, and status reporting. It collects the loader library and the injector binary from their own Gradle modules when building the zip.
- webui/: Web-based user interface components (KernelSU WebUI) for module management and configuration.

## DEVELOPMENT GUIDELINES

1. Code Generation and Modification: Always prioritize stability and security when modifying hooking logic in loader/. Use modern C++ standards as defined in the project's build system. Ensure all new code is free of redundant or unnecessary comments. No nonsense comments.
2. Dependency Management: When interacting with Dobby, LSPlt, or rv64hook, respect their individual licenses and integration patterns.
3. Error Handling: Fail gracefully. If a ZN module fails to load or a hook fails, log the error clearly and ensure both the injector and the host process continue to run without crashing.
4. Rust Code (injector/): Keep every module small and single-purpose; a new file belongs in injector/src/ rather than growing main.rs. Confine FFI to injector/src/sys/ and wrap it in safe interfaces, then run `cargo fmt` and `cargo clippy --all-targets -- -D warnings` for the Android targets before finishing.
5. C++ Code (loader/): Keep every file small and single-purpose; a new file belongs in the loader/src/<area>/ directory it fits rather than growing an existing one. Hook backends stay one-per-file under loader/src/hook/backend/ behind the declarations in loader/src/hook/backend.h, and every index in the ZygiskNextAPI tables is chosen by the module's target_api_version in loader/src/api/table.cpp.

## WORKFLOW

When tasked with a feature implementation or bug fix, follow this strict sequence:

1. Analyze: Understand the request strictly within the context of the "ZN Module Loader" identity.
2. Constraint Check: Explicitly verify that the proposed solution does not violate the Cleanroom Policy or ZN's copyright restrictions (No Picking, No Copying).
3. Design: Outline an original, from-scratch approach to solve the problem. If reverse engineering insights are used, ensure the implementation is purely cleanroom.
4. Implement: Write the code with clear, descriptive variable names.
5. Self-Review: Before outputting the final code, ask yourself: "Is this an independent implementation? Does it mistakenly assume generic Zygisk behavior? Does it contain any extracted logic from ZN?" If yes, revise immediately.
