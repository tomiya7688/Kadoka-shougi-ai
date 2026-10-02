# C++ Safety CI

`.github/workflows/cpp-safety.yml` runs on pull requests, pushes to `main`, and
manual dispatches.

The workflow has two independent checks:

- GCC and Clang builds run the complete CTest suite with AddressSanitizer,
  UndefinedBehaviorSanitizer, libstdc++ assertions, and checked iterators.
- CodeQL builds the CMake targets in manual mode and uploads C/C++ static
  analysis results to GitHub code scanning.

For local sanitizer runs, configure with
`-DKADOKA_ENABLE_SANITIZERS=ON` using GCC or Clang on Linux. The option is off
by default and rejects unsupported platforms or compiler families when enabled.

These checks make memory errors, undefined behavior, checked-container misuse,
and CodeQL findings blocking CI signals. They do not prove the absence of all
defects; they cover the code paths compiled and exercised by the current build
and test suite.
