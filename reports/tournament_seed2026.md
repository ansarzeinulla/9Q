# 9Q fixed-depth tournament

500 start positions x 2 colour orders, 6 random opening plies, seed 2026, threefold repetition = draw.

| Player A | Player B | A wins | B wins | Draws | A score | Elo diff | |
|---|---|---:|---:|---:|---:|---:|---|
| Minimax (D=1) | Random | 100.0% | 0.0% | 0.0% | 100.0% | n/a | 1000 games, 5 s
| Minimax (D=3) | Random | 100.0% | 0.0% | 0.0% | 100.0% | n/a | 1000 games, 6 s
| Minimax (D=3) | Minimax (D=1) | 71.3% | 25.9% | 2.8% | 72.7% | +170 | 1000 games, 7 s
| Minimax (D=5) | Minimax (D=3) | 63.1% | 33.6% | 3.3% | 64.8% | +106 | 1000 games, 31 s
