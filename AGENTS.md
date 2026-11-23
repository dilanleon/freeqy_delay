# Development Workflow

###
## CRITICAL: Use Task Subagents
**USE TASK SUBAGENTS AGGRESSIVELY** to avoid blowing the token budget. This is not optional.
### When to use subagents
- **Verbose output** - Commands producing >50 lines of output (builds, tests, downloads, installs)
- **Multi-step investigations** - Debugging workflows, exploratory research, root cause analysis
- **Infrastructure operations** - VM management, container operations, service restarts
- **Searches with uncertain scope** - Finding files/code when the location isn't known upfront
- **Long-running processes** - Anything that might take significant time or produce streaming output
### Keep in the main thread
- **Planning and coordination** - High-level decisions that need user input
- **Code edits** - File creation and modification
- **Brief status checks** - Quick commands with predictable, short output
- **User communication** - Questions, summaries, handoffs
### Examples
**Use subagent:**
- "Run the full test suite and report failures" → subagent runs tests, returns summary
- "Find where authentication is handled" → subagent searches, returns relevant files
- "Debug why the build is failing" → subagent investigates, returns findings
**Keep in main thread:**
- Reading a specific file you know the path to
- Editing a function
- Running `git status`
- Asking a clarifying question
**IMPORTANT: Scope subagents to task complexity:**
- Do not waste usage on large models for simple tasks. Example: For a verbose output summary, GPT-5.6 Luna on medium effort is fine.
- Scope the complexity of each task assigned to a sub-agent and use varying effort and model size.
- Do not spawn subagents with higher effort or model size than the lead.
## CRITICAL: Search Before Changing
**ALWAYS search for WHY something might be happening before making changes.** Data without context is useless.
When debugging:
1. **Search first** - look for documentation, issues, or prior art explaining the behavior
2. **Understand the mechanism** - don't poke at the system hoping something works
3. **Only then** make targeted changes based on actual understanding

### Two Modes of Work
**Exploration mode** - Gaining awareness of a problem space
- Goal: Understand enough to define validation criteria
- Activities: Research, reading, asking questions, small experiments
- Output: Clear validation criteria for the next phase
- Ask: "Is TDD appropriate here?" before shifting to implementation
**Implementation mode** - Executing against known criteria
- Goal: Achieve a validated outcome
- Prerequisite: Validation criteria are defined and a feedback loop exists
- If fast feedback isn't possible, solve that problem first or flag it
### Transitioning Between Modes
- **Exploration → Implementation**: Shift when you can clearly articulate what success looks like and how to verify it
- **Implementation → Exploration**: Shift back if validation reveals the problem is different than understood, or if repeated failures indicate missing context
- **When uncertain**: Ask the user which mode is appropriate

#### Principles
- **Minimal scope per step** - Keep each cycle small to avoid confusion
- **Substantive checks only** - Tests must verify actual requirements or their dependencies, not just pass for convenience
- **Refactor carefully** - Simplify code between cycles, but never change tested behavior during a refactor
- **Diligence over speed** - Thoroughness matters more than velocity
#### Anti-patterns
- **Mocking internal dependencies** - Never mock your own code; this indicates a testability problem that should be fixed. Mocking is only acceptable for external dependencies (e.g., time, network, filesystem).

# FREEQY DELAY

This file governs contributor workflow and core project invariants. The linked design files
are part of this project contract; keep them current when DSP assumptions, interfaces,
validation requirements, or commands change. `README.md` is the user and build entry point.

## Design index

- [DSP architecture](docs/architecture.md): signal flow, STFT, banks, morph, delay and clearing.
- [Visualization](docs/visualization.md): bucket mapping, analyzers, queue and spectrogram.
- [Editor](docs/editor.md): layout, presets, drawing, point mode and styling.
- [Parameters and state](docs/state.md): stable IDs, defaults and v4 recall.
- [Build and validation](docs/build-and-validation.md): formats, commands and acceptance gates.

## Core invariants

- Keep the host-facing name, bundle and four-character identities stable. Preserve the APVTS root
  `FreeqyDelay` for current saved state.
- No allocation, mutex, I/O, serialization or GUI access during audio processing. Allocate
  worst-case DSP history during prepare; transfer analyzer frames through bounded, preallocated
  SPSC messaging without blocking audio.
- Keep host latency fixed at 2048 samples across all STFT sizes; align dry and bypass paths.
- Preserve all four banks and their saved curves when the active bank count or STFT size changes.
  Never serialize active delay history or visual spectrogram history.

## Function comments

Put a concise header comment immediately above every named C++ function declaration and
definition in `Source/` and `Tests/`, including constructors, destructors, overrides and test
helpers. Explain the function's purpose or a non-obvious invariant; update the comment when
behavior changes. Comment a lambda at its owning statement when its role is not obvious.

## Validation

Always run the build and tests after a change. Develop behavior changes test-first: write a
test for the expected behavior before implementing it. Do not edit a failing test unless the
expected behavior changed. Apply the detailed gates in
[Build and validation](docs/build-and-validation.md); do not claim commercial DAW listening
acceptance or AAX host loading without performing those checks.
