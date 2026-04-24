# Quality Checker Skill

Run comprehensive MemCapture quality checks locally through the chat interface.

## Quick Start

Simply ask Copilot to run quality checks in natural language:

```text
Run quality checks
```

```text
Check memory safety
```

```text
Run static analysis on src
```

## What Gets Checked

1. **Static Analysis**: cppcheck + shellcheck
2. **Memory Safety**: valgrind leak detection
3. **Thread Safety**: helgrind race/deadlock detection
4. **Build Verification**: strict warnings compilation

## Environment

Runs locally using the CMake + vcpkg build system (no Docker required):

- CMake 3.10+, a C++17 compiler, and vcpkg installed
- cppcheck, valgrind, and helgrind available on the host
- Consistent with local developer workflow

## Example Invocations

| What to say | What it does |
| ----------- | ------------ |
| "Run quality checks" | Full suite, summary report |
| "Quick static analysis" | cppcheck + shellcheck only |
| "Check for memory leaks" | valgrind on test binaries |
| "Verify build with strict warnings" | Build with -Werror |
| "Run all checks on src" | Full suite, scoped to MemCapture sources |

## Typical Workflow

1. **Before committing**: "Run static analysis"
2. **Before push**: "Run quality checks"  
3. **Debugging crash**: "Check memory safety"
4. **Reviewing PR**: "Run all checks"

## Output

You'll receive:

- Summary of issues found
- Critical problems highlighted
- Links to detailed reports
- Recommendations for fixes

## Prerequisites

- Docker installed and running
- Access to GitHub Container Registry if the image is not already cached locally

## Tips

- Start with static analysis (fastest)
- Run memory checks after static analysis passes
- Scope checks to changed files for speed
- Full suite before pushing to develop branch
