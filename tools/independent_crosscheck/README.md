# Independent cross-check of Table 2

Perft (`perft_test`) checks the move generator against counts produced by the
same engine. This check is independent: it recomputes Table 2 of the paper
(exhaustive early-game search) with a Togyzkumalak implementation written by a
different author, [yernarsha/togyz_py](https://github.com/yernarsha/togyz_py),
used unmodified.

```bash
git clone https://github.com/yernarsha/togyz_py.git
git -C togyz_py checkout f2c6f9b042031ef7d4df972e29d690d72d485ad1
python3 tools/independent_crosscheck/crosscheck_table2.py --togyz-py togyz_py
```

Result (2026-09-21, Apple M2 Pro): plies 1–8 match the paper exactly in all
seven columns: frontier, continuations, terminals and the four Tuzdyk classes.
That is up to 2,846,239 unique positions and 23,876,487 legal moves at ply 8.
Plies 9–10 are in the script but need tens of GB of RAM in Python.

togyz_py has no license, so it is cited and cloned, not copied into this
repository.
