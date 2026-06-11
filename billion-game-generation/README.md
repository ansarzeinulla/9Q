# Billion-Game Generation

This folder contains the random playout simulator used for the paper's empirical game-tree analysis.

The paper reports:

- `1,000,000,000` simulated random games
- `124,474,599,634` observed board positions
- `871,191,286,723` accumulated legal moves over those positions
- Average random-game length of about `124.474` halfmoves
- Empirical average branching factor of about `7.00`

The included `billion_game_statistics.txt` is the saved billion-game counter file.

## Files

| File | Purpose |
| :--- | :--- |
| `generate_billion_games.cpp` | Random-vs-random simulator and statistics collector. |
| `Makefile` | Builds `generate_billion_games`. |
| `billion_game_statistics.txt` | Included billion-game statistics from the paper run. |
| `../engine/togyzkumalak_rules.*` | Local rules dependency used by the simulator. |
| `../engine/position_hash.*` | Local hashing dependency used by the simulator. |

## Build

```bash
cd FINAL/billion-game-generation
make
```

The Makefile compiles:

```text
generate_billion_games.cpp ../engine/togyzkumalak_rules.cpp ../engine/position_hash.cpp
```

## Run A Small Test

```bash
./generate_billion_games --num=1000 --seed=1 --threads=1 --fresh --stat=sample_billion_game_statistics.txt
```

## Benchmark Simulator Modes

```bash
./generate_billion_games --bench=100000 --seed=1 --threads=10
```

Available modes:

| Mode | Meaning |
| :--- | :--- |
| `env` | Original `ToguzEnv` path with standard RNG. |
| `env-fast-rng` | Original `ToguzEnv` path with SplitMix RNG. |
| `fast` | Compact single-thread simulator. |
| `fast-prebuffer` | Compact simulator with precomputed random values. |
| `fast-parallel` | Compact simulator split across worker threads. This is the default. |

## Reproduce The Billion-Game Run

Use a new output file so the included `billion_game_statistics.txt` is not added to:

```bash
./generate_billion_games --num=1000000000 --seed=709791810521833 --threads=10 --fresh --stat=billion_game_reproduction_statistics.txt
```

Important: the simulator loads existing counters from the stats path, adds the new run, and rewrites the file. If you run the command against the included `billion_game_statistics.txt`, the billion-game counts will be added on top of the existing billion-game counts.

Use `--fresh` when you want to ignore an existing output file and start from zero.

Runtime depends heavily on CPU core count and compiler optimization. The full billion-game run is a long experiment; use small `--num` values first to verify your build.

## Useful Options

```bash
./generate_billion_games --num=1000000 --seed=12345
./generate_billion_games --num=1000000 --threads=10
./generate_billion_games --num=1000000 --max-steps=10000
./generate_billion_games --num=1000000 --fresh --stat=custom_billion_game_statistics.txt
./generate_billion_games --num=1000000 --mode=fast-parallel
```

## Output Notes

- Counters in `billion_game_statistics.txt` are raw counts, not percentages.
- The file starts with a human-readable summary, then keeps the raw `%key value` counters for scripts and exact checks.
- White is `PLAYER_1`, the first player from the initial setup.
- Move-indexed counters use plies/halfmoves: move `1` is White's first move, move `2` is Black's first move, and so on.
- `legalMoveAtPos` uses zero-based pit positions `0..8`.
- `kumalakAtPos` uses `0..7`, because Tuzdyk creation is not legal on the ninth pit under the implemented rule.
- `featureTrackedGames` is the denominator for the advanced feature counters.
