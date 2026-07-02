# Contributing to 9Q

We welcome academic contributions, optimizations, and bug fixes for the Togyzkumalak C++ engine and WebAssembly Web client.

## Code of Conduct

All contributors are expected to adhere strictly to our [Code of Conduct](CODE_OF_CONDUCT.md).

## Development Setup

### Prerequisites

You need a C++17 compliant compiler (`clang++` or `g++`) and standard `make` tools.

### Building Modules

The repository is modularized. To build all executable engines and simulators, run the following commands:

```bash
# Build core search engine
make -C engine

# Build the 1-Billion game random playout simulator
make -C billion-game-generation

# Build the 11-halfmove shortest game solver
make -C shortest-game-generation
```

### Invariant Verification

Before submitting any Pull Request, you must verify that your changes have not introduced rules violations or engine bugs. Run the test suite:

```bash
make test -C engine
```

This runs the Perft validation suite up to depth 4, verifying leaf node invariants against mathematically proven game-state baselines (9, 73, 613, and 5,199 nodes).