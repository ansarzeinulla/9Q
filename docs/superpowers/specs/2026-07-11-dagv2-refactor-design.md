# DAGv1/DAGv2 Search Engine Refactor + 1000-Position Arena

## Context

The Togyzkumalak engine repo has two alpha-beta engines; the newer one, "DAG" (`namespace dag_search`, [include/togyz/dag_search.hpp](include/togyz/dag_search.hpp), [src/dag_search.cpp](src/dag_search.cpp)), is to be preserved as **DAGv1** and copied to **DAGv2**, where all improvements land: unified timed/untimed recursion, killer moves, history heuristic, aspiration windows, and PVS. Alongside, an additive architecture refactor decouples position state, rendering, and heuristic evaluation from `ToguzEnv` — without changing any existing behavior. Acceptance is TDD unit tests plus two head-to-head matches (v1 vs v2, 1000 random positions each played twice with colors swapped): one at **0.25 s/move** (user-reduced from 1 s for runtime) and one at **fixed depth 7**, producing two reports.

**User decisions:** additive shared refactor (ToguzEnv API unchanged, v1 sources untouched except rename); unit tests + match as testing bar; timed 0.25 s/move + depth-7 matches, two reports; **approved one-word exception to "don't touch v1"**: make `global_tt` / `last_stats` `thread_local` (fixes pre-existing multithread data race; bit-identical single-threaded).

## Phase 0 — Baseline pin

- `make build && make test` — record perft + gtest results.
- Save fingerprint: `./build/tg_engine --compare --positions 20 --depth 5 --seed 1337 --noterminal` output.

## Phase 1 — Rename DAG → DAGv1

- `git mv` → [include/togyz/dag_v1_search.hpp](include/togyz/dag_search.hpp) and [src/dag_v1_search.cpp](src/dag_search.cpp); namespace `dag_search` → `dag_v1`.
- Add legacy alias `namespace dag_search = dag_v1;` in the new header and keep a 2-line forwarding header `include/togyz/dag_search.hpp` so `web/wasm/togyz_wasm.cpp` and `tests/test_tt_eviction.cpp` compile unchanged.
- Approved exception: `inline thread_local std::unique_ptr<TranspositionTable> global_tt;` and `thread_local SearchStats last_stats;` (dag_v1 only edits beyond rename).
- CLI [tools/cli_engine/togyzkumalak_engine.cpp](tools/cli_engine/togyzkumalak_engine.cpp): `parse_controller` (~:287) and `timed_bot_move` (~:233) accept `dagv1`/`dagv2`; **match longest prefix first** (`dagv2` currently parses as `dag` + `stoi("v2")` → exception). Keep `dag` as alias for `dagv1`.
- CMake: source list `src/dag_v1_search.cpp`. Rebuild, tests green, fingerprint identical.

## Phase 2 — Additive architecture refactor (before writing v2)

### 2a. EvalParams + HeuristicEvaluator (TDD)
- New test `tests/test_eval_params.cpp` first: default-params `HeuristicEvaluator` == `ToguzEnv::evaluate` (`EXPECT_DOUBLE_EQ`) over ~1000 seeded positions (`setup_balanced_reduced_position` / `setup_random_position`), all perspective/to_play combos + terminals; non-default param changes score.
- [include/togyz/evaluation.hpp](include/togyz/evaluation.hpp): add `struct EvalParams` holding the constants currently inline in `ToguzEnv::evaluate` ([src/togyzkumalak_rules.cpp:400-451](src/togyzkumalak_rules.cpp)): tuzdyk 650/70, kazan 108/0.28/+18@<30, material 18/4@<24, mobility 18, lone_tuzdyk 170, tempo 10, win_score 1e6. `HeuristicEvaluator(EvalParams = {})`; `cache_key()` mixes params when non-default (keeps `0x5354...` for defaults).
- [src/evaluation.cpp](src/evaluation.cpp): move `evaluate` body here **verbatim** (preserve expression order — double math is order-sensitive), literals → `params_`.
- `ToguzEnv::evaluate` becomes a wrapper delegating to a local `static const HeuristicEvaluator kDefault;` — v1/minimax stay bit-identical.

### 2b. Console renderer
- New `include/togyz/console_render.hpp` + `src/console_render.cpp`, `namespace togyz_render`: `render_console(std::ostream&, ...)` — body moved verbatim from rules.cpp:453-476.
- `ToguzEnv::render` becomes a deprecated thin forwarder to it; CLI call sites unmigrated (smallest diff).

### 2c. PositionState
- New header-only `include/togyz/position_state.hpp`: `struct PositionState { Bitboard board; std::array<int,2> kazans, tuzduks; int to_play; }` + `capture_position(const ToguzEnv&)`. Used internally by dag_v2; ToguzEnv untouched.
- CMake: add `src/console_render.cpp` to `togyz_core`; add `test_eval_params`. All green; fingerprint unchanged.

## Phase 3 — DAGv2 skeleton + equivalence pin (tests first)

- New `include/togyz/dag_v2_search.hpp` + `src/dag_v2_search.cpp`, namespace `dag_v2` — verbatim copy of v1 (own TT with `thread_local` global, own helpers; sharing would touch v1).
- Public: `struct SearchOptions { bool use_killers, use_history, use_aspiration, use_pvs; static SearchOptions v1_baseline(); }` (all false in baseline); `get_best_move(env, depth, evaluator, opts)`, `get_best_move_timed(...)`, `get_last_stats()`.
- Header-exposed testable components: `KillerTable` (2 slots × MAX_PLY=128, record shifts/dedupes, never TT move), `HistoryTable` (`[player][pit]`, `reward += depth*depth`, halve-all on overflow), `AspirationWindow` (initial(prev, δ=50), widen_fail_low/high with growing δ → full window; `is_mate_bound()` for |score| in the WIN/TT-clamp band ⇒ full window immediately).
- Tests written BEFORE integration (new CMake targets):
  1. `tests/test_dag_v2_tables.cpp` — killer/history semantics.
  2. `tests/test_dag_v2_aspiration.cpp` — window sequences, mate-bound bypass.
  3. `tests/test_dag_v2_equivalence.cpp` — for seeds {1337,42,7} × ~30 positions × depths 1–5 with TTs re-initialized per call: `dag_v2` baseline best move, root evals (`EXPECT_DOUBLE_EQ`), **and node counts** == `dag_v1`. Plus PVS-only == baseline root score (and move; relax to score-only with comment if tie-flips flake), and each single feature on ⇒ same root score at fixed depth.

## Phase 4 — DAGv2 features (one per commit, all tests green after each)

1. **Unify timed/untimed**: one iterative-deepening driver over the (already deadline-aware) `search_root`; untimed = deadline nullptr; root ordering unified via `order_moves` seeded with previous iteration's best. `v1_baseline()` bypasses ID (single `search_root` at target depth) so the fixed-depth equivalence pin stays exact (ID's TT priming would otherwise legitimately change results).
2. **Killers**: `KillerTable` member, `ply` threaded through `search()`; record quiet moves (`move_order_key ≥ 2`) on beta cutoff. Ordering bands: TT (100000) > tactical (key ≤ 1) > killer0/1 (500/499) > quiets — killers must NOT outrank captures (this game is capture-heavy).
3. **History**: reward quiet cutoffs; quiet ordering score = base + history clamped into 0..400 (below the killer band).
4. **Aspiration**: depth 1 full window; depth ≥ 2 window around prev score; widen on root fail-low/high; mate-bound ⇒ full window; keep v1's break on `value >= WIN_SCORE`. Rebalance repetition `history_stack` (enter/leave) between re-search attempts.
5. **PVS**: first move full window, rest null-window `(-alpha-1, -alpha)`, re-search on `alpha < v < beta`. Restore board/kazan/tuzduk/history-stack between null-window and re-search (existing backup/restore pattern). Add debug assertion that `history_stack.size()` is restored after each move-loop iteration.

After each feature: unit + equivalence suites + 20-position depth-5 v1-vs-v2 smoke (v2 must not regress; node counts should drop for ordering features).

## Phase 5 — Arena mode in the CLI

In [tools/cli_engine/togyzkumalak_engine.cpp](tools/cli_engine/togyzkumalak_engine.cpp):
- Extend `timed_bot_move` + new `fixed_depth_move` path for kinds `dagv1`/`dagv2` (record nodes + completed_depth from each namespace's `get_last_stats()`).
- New `--arena` mode with flags: `--p1bot dagv1 --p2bot dagv2 --positions 1000 --seed 1337 --threads 10 --move-time 0.25 --report-file <path>`; `--fixed-depth 7` switches to fixed-depth mode.
- `run_engine_arena`: reuse `make_balanced_start_positions` (~:640); build 2000 tasks (each position twice, colors swapped); distribute via `std::atomic<size_t>` next-task index over worker threads (avoids chunk skew from long games). Per game: winner mapped through color assignment, plies, per-engine nodes/avg depth; draws split repetition vs max-step.
- Report to `--report-file` and stdout: config header; per-engine W/L/D + %; per-color breakdown (asymmetry check); avg plies; avg completed depth and nodes/move per engine; wall time. Reports in `reports/` (auto-created).

## Phase 6 — Run the two matches

Calibrate with `--positions 10 --threads 1` first, then:

```
# 1) Fixed depth 7 (~20 min–2 h wall at 10 threads) — run first
nohup ./build/tg_engine --arena --p1bot dagv1 --p2bot dagv2 --positions 1000 \
  --seed 1337 --threads 10 --fixed-depth 7 \
  --report-file reports/arena_depth7.txt > reports/arena_depth7.log 2>&1 &

# 2) Timed 0.25 s/move (~1.5 h wall at 10 threads)
nohup ./build/tg_engine --arena --p1bot dagv1 --p2bot dagv2 --positions 1000 \
  --seed 1337 --threads 10 --move-time 0.25 \
  --report-file reports/arena_timed_0.25s.txt > reports/arena_timed.log 2>&1 &
```

Deliverable: the two report files summarized to the user (per-engine W/L/D, color breakdown, depth/node stats).

## Verification

- `make build && make test` green at end of every phase (perft, zobrist, tt_eviction, evaluation, + 4 new test targets).
- Phase 0 `--compare` fingerprint re-run after Phases 1–2: byte-identical (proves v1/minimax/eval untouched behaviorally).
- Equivalence suite pins v2-baseline == v1 exactly (moves, scores, node counts).
- Feature smoke matches show node reductions and no strength regression.
- Final acceptance: the two 1000-position arena reports.

## Risks

- **Aspiration vs WIN_SCORE/TT int16 clamp** (TT stores clamp to ±32767 at dag_search.cpp:260): mate-band scores ⇒ full window immediately.
- **Killer/history above captures** would explode nodes — band order enforced and checked via node counts.
- **Repetition enter/leave imbalance** across PVS/aspiration re-searches silently corrupts draw detection — debug assertion added.
- **Float bit-identity** in eval migration — caught by `EXPECT_DOUBLE_EQ` cross-check test.
- **`parse_controller` prefix collision** for `dagv2` — longest-prefix-first parsing.
