# Shortest-Game Generation

*Read this in other languages: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

This folder contains the shortest terminal-game search used for the paper's exhaustive early-game result.

The paper reports that no terminal state exists before halfmove `11`, and gives one terminal line at halfmove `11` with final Kazan score `88-10` for White/P1.

## Files

| File | Purpose |
| :--- | :--- |
| `find_shortest_game.cpp` | Standalone rules simulator and shortest-game searcher. |
| `Makefile` | Builds `find_shortest_game`. |
| `shortest_terminal_game.txt` | Saved 11-halfmove terminal line. |
| `shortest_candidate_replay.tsv` | Replay table for the saved candidate line. |
| `shortest_depth_proof.tsv` | Saved breadth-search statistics through the proof layers. |

This component is standalone and does not depend on the `engine/` folder.

## Build

```bash
cd FINAL/shortest-game-generation
make
```

## Verify The Saved Candidate

```bash
./find_shortest_game --verify-only
```

This replays the saved 11-halfmove line and checks that it is terminal. It rewrites `shortest_terminal_game.txt` and `shortest_candidate_replay.tsv`.

## Run The Full Search And Proof

```bash
./find_shortest_game
```

This does three things:

1. Searches for a terminal line within the configured depth.
2. Proves that no terminal edge exists before halfmove `11`.
3. Regenerates `shortest_terminal_game.txt`, `shortest_candidate_replay.tsv`, and `shortest_depth_proof.tsv`.

The proof step materializes very large frontiers and can take significant time and memory.

## Saved Shortest Line

`shortest_terminal_game.txt` contains:

```text
1. 98 22
2. 87 12
3. 76 25
4. 65 13
5. 93 25
6. 54
```

Move notation is `selected_pit``landing_pit`, both one-based. For example, `98` means the player selected pit `9` and the final sown kumalak landed in pit `8`. If a move creates a Tuzdyk, the notation appends `x`.

The final move is White/P1's 11th halfmove of the game and ends with Kazans `88-10`.

## Optional Arguments

```bash
./find_shortest_game --verify-only
./find_shortest_game --no-proof
./find_shortest_game --max-depth 11 --beam 1000
```

`--verify-only` is the fastest reproducibility check. The default command is the full proof-oriented run.
