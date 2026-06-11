# Hand-Written Evaluation Engine

*Read this in other languages: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

This folder contains the C++ Togyzkumalak rules engine and playable command-line engine.

For the research package, the main required engine is the hand-written minimax engine:

- Bot name: `aiN`
- Source: `minimax_engine.cpp` and `minimax_engine.hpp`
- Search: fixed-depth minimax with alpha-beta pruning, move ordering, transposition table, and tactical quiescence search
- Evaluation: hand-written heuristic evaluator exposed through `HeuristicEvaluator` and `ToguzEnv::evaluate`

The folder also includes the newer `dagN` / `filterN` search implementation, but `aiN` is the minimax/evaluation engine used as the main baseline.

## Files

| File | Purpose |
| :--- | :--- |
| `togyzkumalak_rules.*` | Board representation, legal moves, rules, terminal detection, repetition tracking, and heuristic evaluation. |
| `minimax_engine.*` | Human-crafted minimax/quiescence engine. |
| `dag_search.*` | DAG/filter search engine included for comparison. |
| `evaluation.*` | Evaluator interface plus the statistics-informed heuristic wrapper. |
| `position_hash.*` | Hashing for transposition/repetition logic. |
| `togyzkumalak_engine.cpp` | CLI entry point for play, self-play, comparison, benchmark, and UCI-like commands. |
| `Makefile` | Builds the `togyzkumalak_engine` binary. |

## Build

```bash
cd FINAL/engine
make
```

To use `g++` instead of `clang++`:

```bash
make CXX=g++
```

## Play Against The Minimax Engine

```bash
./togyzkumalak_engine --mode human-ai
```

The engine uses iterative deepening with a clock. By default each half-move gets
`0.01` seconds. Change the budget with `--move-time`:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.05
```

## Run Automated Games

```bash
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --switch-color --move-time 0.01
```

## Bot Names

| Bot | Meaning |
| :--- | :--- |
| `ai` / `aiN` | Hand-written minimax/quiescence engine using the per-move clock; `N` is accepted for old command compatibility but ordinary games are time-controlled. |
| `dag` / `dagN` | DAG/filter search using the per-move clock in ordinary games. |
| `filter` / `filterN` | Alias for `dag` / `dagN`. |
| `randombot` | Random legal move bot. |
| `human` | Terminal human player. |

Modes use `P1-P2` format:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.01
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal --move-time 0.01
./togyzkumalak_engine --mode ai-dag --numgames 10 --noterminal --move-time 0.01
```

## Compare Minimax And DAG Search

```bash
./togyzkumalak_engine --compare --depth 4 --positions 100 --seed 1337 --noterminal
```

This explicit comparison mode is still fixed-depth and tests hand-written minimax
`aiN` against `dagN` on the same randomized balanced starts.

## Benchmark Minimax Self-Play

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 100 --threads 1 --noterminal --move-time 0.01
./togyzkumalak_engine --benchmark --bot minimax --positions 100 --threads 1 --noterminal --move-time 0.01
```

Use `--threads 0` for automatic parallelism:

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 1000 --threads 0 --noterminal --move-time 0.01
```

The benchmark reports total time, seconds per game, milliseconds per move, game length, result counts, repetition draws, and max-step games.

## Heuristic Evaluation

The evaluator is implemented in `ToguzEnv::evaluate` and wrapped by `HeuristicEvaluator`. It scores:

- terminal wins, losses, and draws
- Kazan score difference, with a phase-adjusted race weight
- side material and a single playable-pit count
- compact Tuzdyk ownership and location bonuses
- low-material sweep pressure and a small side-to-move bonus

The minimax engine uses this evaluator by default:

```bash
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --move-time 0.01 --p1 heuristic --p2 heuristic
```

## Generated Files

- `limit.txt` can be created during benchmarks. It contains `0` for a clean run or a trace if a game reaches the max-step safety cap.
- Dataset JSONL files are created only if you pass `--dataset PATH`.

No model file is required for the hand-written minimax engine.
