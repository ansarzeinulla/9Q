# 9Q Engine TODO

Source: engine review and optimization notes dated 2026-06-13.

Legend:
- [x] done in the current implementation pass
- [ ] not started
- P0: must fix before trusted experiments or publication numbers
- P1: high-return performance and memory work
- P2: architecture and maintainability
- P3: interface hardening and smaller polish

## P0 - Search Correctness And Experiment Trust

- [x] T001 Add one shared `landing_pit(start, stones)` helper in `togyzkumalak_rules.hpp`.
  - Findings: F1, F13
  - Acceptance: all current landing-pit formulas use the helper or intentionally document why they do not.

- [x] T002 Fix minimax `move_order_key` to use `landing_pit`.
  - Findings: F1
  - Acceptance: multi-stone moves land at `start + stones - 1`; qsearch no longer uses the off-by-one target.

- [x] T003 Update DAG move ordering and WASM move notation to call `landing_pit`.
  - Findings: F13
  - Acceptance: no duplicate landing formula remains in DAG or WASM notation code.

- [x] T004 Add a focused `landing_pit` regression test.
  - Cover all 18 starts and representative stone counts: 1, 2, 9, 18, 19, 81, 162.
  - Acceptance: test fails on the old minimax formula for every multi-stone case.

- [x] T005 Add tactical-classification regression tests for minimax qsearch.
  - Include capture, tuzdyk creation, quiet move, and empty-landing cases.
  - Acceptance: moves that actually capture or create a tuzdyk are included in qsearch.

- [x] T006 Route CLI `dag` timed moves through `dag_search::get_best_move_timed`.
  - Findings: F4
  - Acceptance: the CLI no longer performs a separate depth loop with deadline checks only between depths.

- [ ] T007 Add a DAG time-budget regression check.
  - Run a short benchmark at 10 ms/move and verify mean elapsed time stays near the requested budget.
  - Acceptance: no repeated +50 percent budget overrun in the driver path.

- [x] T008 Add a minimal native test harness under `tests/`.
  - Prefer a dependency-free C++ test binary first.
  - Acceptance: `make test` or an equivalent script builds and runs the rules/search smoke tests.

- [x] T009 Add random-game invariant tests.
  - Check total stones on board plus kazans is always 162.
  - Check generated moves are legal for the current side.
  - Check each player has at most one tuzdyk.
  - Check no tuzdyk is ever created in the ninth pit.
  - Check symmetric tuzdyks never coexist.

- [ ] T010 Add perft-style move-count baselines from the initial position.
  - Record leaf counts for depths 1 through 6.
  - Acceptance: any future rule/generator change must explicitly update the baseline.

- [ ] T011 Add a golden game covering tuzdyk creation.
  - Include the exact move list, kazan counts, tuzdyk positions, side to move, and terminal flag after each step.

- [ ] T012 Add a golden game covering even capture.
  - Include both one-stone and multi-stone sowing moves.

- [x] T013 Add an "atsyz kalu" terminal-position test.
  - Acceptance: a side with no legal moves triggers final stone collection and winner determination.

- [ ] T014 Add a threefold-repetition test for `step`.
  - Acceptance: the third visit sets `winner_code == -1`.

- [x] T015 Add `step` versus `step_search` equivalence tests.
  - Generate random reachable positions, apply every legal move both ways, and compare board, kazans, tuzdyks, side to move, winner, and terminal flag.

- [x] T016 Add Zobrist consistency tests.
  - Acceptance: incremental hash after `step_search` equals full recomputation for all tested moves.

- [ ] T017 Add tactical search-position tests.
  - Include obvious one-ply captures to 82, forced tuzdyk creation, and a quiet non-tactical control.
  - Acceptance: minimax and DAG find the required move at fixed shallow depths.

- [x] T018 Add CI for native engine builds.
  - Matrix: GCC and Clang, C++17, `-Wall -Wextra`.
  - Acceptance: CI runs tests on every push and pull request.

- [ ] T019 Add sanitizer CI.
  - AddressSanitizer and UndefinedBehaviorSanitizer on a short test suite.
  - ThreadSanitizer for explicitly multithreaded benchmark smoke tests once F3 is fixed.
  - Status: ASan/UBSan workflow step is present; TSan remains pending with the shared minimax TT race.

## P0 - Minimax Transposition Table Correctness

- [ ] T020 Add `Bound { Exact, Lower, Upper }` to minimax TT entries.
  - Findings: F2
  - Acceptance: TT values from cutoffs are not treated as exact scores.

- [ ] T021 Store `alpha_original` in minimax nodes before search.
  - Acceptance: stored bound type is selected from `value <= alpha_original`, `value >= beta`, otherwise exact.

- [ ] T022 Rewrite minimax TT probe logic.
  - Exact entries return immediately.
  - Lower entries return only when `value >= beta`.
  - Upper entries return only when `value <= alpha`.
  - Otherwise the entry may only contribute move-ordering data.

- [ ] T023 Add best-move storage to minimax TT.
  - Findings: F17
  - Acceptance: TT entry includes a move index or sentinel.

- [ ] T024 Use TT best move as the first root and interior ordering candidate.
  - Acceptance: if the TT move is legal, it is searched before the tactical sort bucket.

- [ ] T025 Add a TT alpha-beta correctness regression.
  - Build a small deterministic position set where the previous no-bound TT can return a wrong value.

- [ ] T026 Compact minimax TT entries.
  - Candidate shape: combined key, float score, depth byte, bound byte, best move byte, perspective byte.
  - Acceptance: entry size is materially smaller than the current native 32-byte record.

- [ ] T027 Add a TT stats report.
  - Track probes, exact hits, lower hits, upper hits, stores, overwrites, and illegal stored moves.

- [ ] T028 Re-run fixed-depth `--compare` after TT bound support.
  - Acceptance: store the command, commit hash, and headline numbers in a benchmark note.

## P0 - Thread Safety

- [x] T029 Make sow-mask initialization thread-safe.
  - Findings: F3
  - Use local static initialization or `std::call_once`.
  - Acceptance: concurrent first construction of `ToguzEnv` has no data race.

- [x] T030 Audit `Zobrist::init` for thread-safe one-time initialization.
  - Acceptance: TSan reports no Zobrist initialization race.

- [ ] T031 Replace global minimax TT sharing in multithreaded modes.
  - Preferred first step: `thread_local` TT.
  - Acceptance: `--benchmark --bot ai --threads 4` has no TT data race.

- [ ] T032 Add a memory cap for per-thread TT.
  - Acceptance: default total memory is reasonable for 4 to 8 worker threads.

- [ ] T033 Remove or protect DAG `last_stats`.
  - Findings: F3 follow-up
  - Acceptance: stats retrieval cannot race between simultaneous DAG searches.

- [ ] T034 Add an explicit warning or fallback while multithreaded minimax is unsafe.
  - Temporary guard only if T031 is not done yet.

- [ ] T035 Run and archive a ThreadSanitizer verification command.
  - Acceptance: include command and clean result in the development notes.

## P1 - Move Application And Hash Performance

- [ ] T036 Split `step_search` into hash and no-hash paths.
  - Findings: F7
  - Candidate API: `step_search` and `step_search_nohash`, or templated internal implementation.

- [ ] T037 Switch qsearch callers that ignore hashes to no-hash move application.
  - Acceptance: minimax qsearch no longer pays incremental Zobrist cost with a dummy hash.

- [ ] T038 Switch DAG search to no-hash move application plus full hash recomputation only when needed.
  - Acceptance: repetition checks still use exact full hashes.

- [ ] T039 Benchmark step application before and after no-hash path.
  - Acceptance: report ns/move for hash, no-hash, and full recompute variants.

- [ ] T040 Re-run engine benchmark after no-hash path.
  - Acceptance: compare nodes/sec, ms/move, and completed depth for ai and dag.

## P1 - Memory Footprint

- [x] T041 Lazily allocate the minimax TT on first minimax search.
  - Findings: F6
  - Acceptance: a UCI session that quits without searching does not allocate hundreds of MB.

- [ ] T042 Add CLI `--tt-bits N`.
  - Acceptance: native users can choose TT size without recompiling.

- [ ] T043 Add UCI `Hash` option or documented equivalent.
  - Acceptance: GUI users can control minimax TT memory.

- [ ] T044 Keep native and WASM TT defaults separate.
  - Acceptance: WASM remains conservative; native can keep a stronger default.

- [ ] T045 Replace `ToguzEnv::random_buffer` with shared or lightweight RNG state.
  - Findings: F14
  - Acceptance: copying `ToguzEnv` no longer copies a 100,000-int buffer.

- [ ] T046 Measure `sizeof(ToguzEnv)` before and after RNG extraction.
  - Acceptance: size reduction is documented in the benchmark note.

## P1 - Repetition-Aware Minimax

- [ ] T047 Port DAG repetition tracking into minimax search.
  - Findings: F5
  - Acceptance: a third repetition inside the search tree evaluates as draw.

- [ ] T048 Disable or qualify minimax TT use under repetition pressure.
  - Acceptance: TT entries that ignore path-dependent repetition cannot corrupt repetition-sensitive nodes.

- [ ] T049 Add repetition search tests.
  - Include a position where the best-looking material line repeats into a draw.

- [ ] T050 Compare minimax and DAG on repetition-heavy positions.
  - Acceptance: both searches agree on draw-valued forced repetition lines.

## P2 - Search Maintainability And Strength

- [ ] T051 Unify timed and untimed minimax recursion.
  - Findings: F13
  - Acceptance: one minimax implementation handles both deadline and no-deadline modes.

- [ ] T052 Add amortized deadline checks.
  - Candidate: check every 1024 nodes instead of calling the clock at every node.

- [ ] T053 Unify timed and untimed qsearch recursion.
  - Acceptance: one qsearch implementation handles both paths.

- [ ] T054 Add killer-move ordering.
  - Two killer slots per depth.
  - Acceptance: quiet beta-cutting moves are promoted in later sibling nodes.

- [ ] T055 Add history-heuristic ordering.
  - Acceptance: non-capture moves that cause cutoffs gain ordering weight.

- [ ] T056 Add aspiration windows after TT bound support.
  - Acceptance: iterative deepening starts near the previous iteration score and widens on fail-high/fail-low.

- [ ] T057 Add principal variation search after TT bound support.
  - Acceptance: non-PV moves search with a null window first.

- [ ] T058 Add benchmark output for completed depth.
  - Acceptance: timed searches report depth distribution, not only elapsed time.

- [ ] T059 Add a stable benchmark suite.
  - Use fixed seed, fixed reduced-position generator parameters, and fixed position count.

## P2 - Rules And Evaluation Architecture

- [ ] T060 Rename `uint128` alias or switch to unsigned 128-bit type.
  - Findings: F8
  - Acceptance: type name matches signedness and bit operations remain sanitizer-clean.

- [ ] T061 Document `step` preconditions.
  - Findings: F9
  - Acceptance: callers know whether they must prevalidate legal moves.

- [ ] T062 Add a safe move-application wrapper.
  - Acceptance: external protocols can reject illegal moves without mutating state.

- [ ] T063 Split lightweight position state from full `ToguzEnv`.
  - Findings: F14
  - Acceptance: search code can copy a small state without session-only fields.

- [ ] T064 Move rendering out of `ToguzEnv`.
  - Acceptance: console rendering is not mixed into core rules state.

- [ ] T065 Move heuristic constants into `EvalParams`.
  - Findings: F15
  - Acceptance: constants are named, documented, and injectable.

- [ ] T066 Move heuristic implementation fully into `HeuristicEvaluator`.
  - Acceptance: `ToguzEnv` no longer owns evaluation policy.

- [ ] T067 Add evaluation ablation switches.
  - Acceptance: kazan, tuzdyk, material, mobility, monopoly, and tempo terms can be measured independently.

- [ ] T068 Document the evaluation tuning method.
  - Acceptance: README or engine docs say whether values were hand-tuned, self-play tuned, or provisional.

## P3 - CLI And UCI Robustness

- [ ] T069 Make UCI `isready` always answer `readyok`.
  - Findings: F10
  - Acceptance: GUI startup does not hang before `ucinewgame`.

- [ ] T070 Validate UCI `position ... moves` before applying them.
  - Findings: F9
  - Acceptance: illegal moves produce a clear diagnostic and do not mutate the game state.

- [ ] T071 Replace raw `stoi`/`stod`/`stoull` parsing with checked helpers.
  - Findings: F11
  - Acceptance: invalid numeric CLI input exits cleanly or reports an error instead of terminating.

- [ ] T072 Harden UCI numeric parsing.
  - Acceptance: malformed `go movetime`, `go depth`, and similar inputs do not throw uncaught exceptions.

- [ ] T073 Make `--mode` parsing reject unknown one-token modes.
  - Findings: F19
  - Acceptance: `--mode nonsense` is an error, not "nonsense versus nonsense".

- [ ] T074 Document the supported UCI dialect.
  - Acceptance: engine README distinguishes full UCI support from the current UCI-like command subset.

- [ ] T075 Add CLI smoke tests for bad arguments.
  - Include bad depth, bad seed, unknown mode, and illegal UCI move.

## P3 - WASM And Browser Integration

- [ ] T076 Reject tuzdyk in the ninth pit while parsing FEN.
  - Findings: F18
  - Acceptance: invalid FEN returns an error instead of accepting an unreachable position.

- [ ] T077 Make unknown WASM bot names errors.
  - Findings: F18
  - Acceptance: `normalize_bot` no longer silently falls back to `randombot`.

- [ ] T078 Finalize terminal status after `tg_set_fen`.
  - Acceptance: a FEN with no legal move for the side to move is immediately marked terminal or rejected with a documented error.

- [ ] T079 Add WASM parser tests.
  - Cover stone totals, kazan ranges, tuzdyk constraints, side to move, no-move positions, and unknown bot names.

- [ ] T080 Rebuild committed WASM artifacts after engine API changes.
  - Acceptance: `web/public/wasm/togyz_engine.js` and `.wasm` match the C++ source when required.

## P3 - Smaller Cleanup And Instrumentation

- [ ] T081 Make qsearch consistently fail-soft.
  - Findings: F12
  - Acceptance: qsearch returns the best known score, not artificial alpha/beta sentinels.

- [ ] T082 Make benchmark history snapshots lazy.
  - Findings: F20
  - Acceptance: board snapshots are only built when limit tracing is requested or needed.

- [ ] T083 Add `--limit-trace` to self-play benchmarks.
  - Acceptance: expensive trace data is opt-in.

- [ ] T084 Add a benchmark result template.
  - Include command, compiler, CPU, TT bits, threads, seed, positions, mean time, max time, and completed depth.

- [ ] T085 Add engine documentation for DAG versus minimax.
  - Acceptance: README explains why both searches exist and which one is experimental.

- [ ] T086 Add developer notes for reproducible experiments.
  - Acceptance: exact commands for `--benchmark`, `--compare`, sanitizer runs, and WASM build are collected in one place.

## Suggested Implementation Order

1. Finish P0 formula and DAG time-control verification: T004, T005, T007.
2. Add the native test harness and CI: T008 through T019.
3. Fix minimax TT correctness: T020 through T028.
4. Remove thread-safety hazards: T029 through T035.
5. Take the large performance wins: T036 through T046.
6. Make minimax repetition-aware: T047 through T050.
7. Proceed with architecture, interface hardening, and deeper search improvements.
