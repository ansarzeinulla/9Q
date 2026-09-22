# Shortest-game results (Tables 1 and 2 of the paper)

Output of `tg_shortest_solver` (source: `tools/shortest_game_solver/find_shortest_game.cpp`), as used in the paper.

| File | What |
| :--- | :--- |
| `shortest_depth_proof.tsv` | Exhaustive breadth-first search, half-moves 1–10: frontier positions, legal moves, terminal moves (all zero), unique positions after each half-move, Kazan and Tuzdyk statistics, time. This is the data behind Table 2. |
| `shortest_terminal_game.txt` | An 11-half-move game that ends the game (Table 1). |
| `shortest_candidate_replay.tsv` | The same game replayed move by move: Kazans, stones on each side, Tuzdyks, result. |

To regenerate them, build the project (see the main README) and run from the directory where the files should be written:

```bash
./build/tg_shortest_solver --no-proof   # writes shortest_terminal_game.txt and shortest_candidate_replay.tsv
./build/tg_shortest_solver              # also runs the exhaustive search and writes shortest_depth_proof.tsv; memory-heavy
```

The two game files regenerate byte for byte (checked 2026-09-22). In the proof table, only the `elapsed_seconds` column should change between runs.
