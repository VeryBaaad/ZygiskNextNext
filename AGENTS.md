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

- loader/: The core native component. Responsible for process injection, module loading, and hook management using Dobby (inline hooking) or rv64hook (inline hooking for riscv64), and LSPlt (PLT hooking).
- module/: The Magisk / KernelSU / APatch module wrapper. Handles installation, environment setup, permission management, and status reporting.
- webui/: Web-based user interface components (KernelSU WebUI) for module management and configuration.

## DEVELOPMENT GUIDELINES

1. Code Generation and Modification: Always prioritize stability and security when modifying hooking logic in loader/. Use modern C++ standards as defined in the project's build system. Ensure all new code is free of redundant or unnecessary comments. No nonsense comments.
2. Dependency Management: When interacting with Dobby, LSPlt, or rv64hook, respect their individual licenses and integration patterns.
3. Error Handling: Fail gracefully. If a ZN module fails to load or a hook fails, log the error clearly and ensure both the injector and the host process continue to run without crashing.

## WORKFLOW

When tasked with a feature implementation or bug fix, follow this strict sequence:

1. Analyze: Understand the request strictly within the context of the "ZN Module Loader" identity.
2. Constraint Check: Explicitly verify that the proposed solution does not violate the Cleanroom Policy or ZN's copyright restrictions (No Picking, No Copying).
3. Design: Outline an original, from-scratch approach to solve the problem. If reverse engineering insights are used, ensure the implementation is purely cleanroom.
4. Implement: Write the code with clear, descriptive variable names.
5. Self-Review: Before outputting the final code, ask yourself: "Is this an independent implementation? Does it mistakenly assume generic Zygisk behavior? Does it contain any extracted logic from ZN?" If yes, revise immediately.
