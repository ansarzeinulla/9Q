#include "togyz/togyzkumalak_rules.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

constexpr int OUTCOME_WHITEWIN = 0;
constexpr int OUTCOME_DRAW = 1;
constexpr int OUTCOME_BLACKWIN = 2;
constexpr int OUTCOME_COUNT = 3;

constexpr int TUZDYK_STATE_NONE = 0;
constexpr int TUZDYK_STATE_ONLY_WHITE = 1;
constexpr int TUZDYK_STATE_ONLY_BLACK = 2;
constexpr int TUZDYK_STATE_BOTH = 3;
constexpr int TUZDYK_STATE_COUNT = 4;

constexpr int KUMALAK_POS_COUNT = 8;
constexpr int LEGAL_POS_COUNT = 9;
constexpr int OPENING_PAIR_COUNT = LEGAL_POS_COUNT * LEGAL_POS_COUNT;
constexpr int BRANCHING_BAND_COUNT = 3;
constexpr int TRACKED_MOVE_LIMIT = 10000;
constexpr uint64_t SPLITMIX_GAMMA = 0x9e3779b97f4a7c15ULL;

static inline uint64_t splitmix64_mix(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

struct Args {
  uint64_t num_games = 1;
  uint64_t seed = 0;
  int max_steps = 10000;
  int threads = 0;
  bool benchmark = false;
  bool format_only = false;
  bool fresh_output = false;
  std::string mode = "fast-parallel";
  fs::path stat_path;
};

// The simulator tracks the exact counters used in the paper. The field names
// intentionally stay close to the published tables so that the raw output can
// be checked or re-aggregated without guessing what each value means.
struct Stats {
  uint64_t games = 0;
  uint64_t whitewin = 0;
  uint64_t draw = 0;
  uint64_t blackwin = 0;

  uint64_t withNoKumalaks = 0;
  uint64_t withWhiteKumalakONLY = 0;
  uint64_t withBlackKumalakONLY = 0;
  uint64_t withBOTHKumalaks = 0;
  std::array<uint64_t, KUMALAK_POS_COUNT> kumalakAtPos{};

  uint64_t moments_where_one_player_had_no_legal_move = 0;
  uint64_t moves_of_all_games = 0;
  uint64_t positions_observed = 0;
  uint64_t sum_of_all_legal_moves_per_position = 0;
  std::array<uint64_t, LEGAL_POS_COUNT> legalMoveAtPos{};

  uint64_t max_step_games = 0;
  uint64_t longest_game = 0;

  uint64_t feature_tracked_games = 0;
  uint64_t feature_move_sum = 0;
  uint64_t feature_min_moves = 0;
  uint64_t feature_min_moves_to_win = 0;
  uint64_t feature_max_moves = 0;

  std::array<std::array<uint64_t, OUTCOME_COUNT>, TUZDYK_STATE_COUNT>
      tuzdykStateOutcome{};
  std::array<std::array<uint64_t, OUTCOME_COUNT>, KUMALAK_POS_COUNT>
      whiteTuzdykPosOutcome{};
  std::array<std::array<uint64_t, OUTCOME_COUNT>, KUMALAK_POS_COUNT>
      blackTuzdykPosOutcome{};
  std::array<uint64_t, KUMALAK_POS_COUNT> whiteTuzdykYield{};
  std::array<uint64_t, KUMALAK_POS_COUNT> blackTuzdykYield{};
  std::array<uint64_t, 2> tuzdykBlockedBySymmetry{};
  std::array<uint64_t, TRACKED_MOVE_LIMIT + 1> whiteTuzdykCreatedAtMove{};
  std::array<uint64_t, TRACKED_MOVE_LIMIT + 1> blackTuzdykCreatedAtMove{};
  std::array<uint64_t, 2> tuzdykCreatedAtMoveOverflow{};

  std::array<std::array<uint64_t, OUTCOME_COUNT>, LEGAL_POS_COUNT>
      whiteFirstMoveOutcome{};
  std::array<std::array<uint64_t, OUTCOME_COUNT>, OPENING_PAIR_COUNT>
      openingPairOutcome{};

  std::array<std::array<uint64_t, OUTCOME_COUNT>, 2> atsyrauByPlayerOutcome{};
  std::array<uint64_t, 2> atsyrauOpponentCaptureSum{};

  uint64_t decisive_games = 0;
  uint64_t sum_winning_score = 0;
  uint64_t sum_losing_score = 0;
  uint64_t close_wins_margin_0_to_5 = 0;
  uint64_t blowout_wins_margin_50_plus = 0;
  std::array<uint64_t, TOTAL_STONES + 1> winMargin{};

  std::array<uint64_t, TRACKED_MOVE_LIMIT + 1> gameLengthMoves{};
  uint64_t gameLengthMovesOverflow = 0;

  std::array<uint64_t, BRANCHING_BAND_COUNT> branchingPositions{};
  std::array<uint64_t, BRANCHING_BAND_COUNT> branchingLegalMoveSum{};
  uint64_t exactMoveTrackedGames = 0;
  std::vector<uint64_t> branchingExactMovePositions =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> branchingExactMoveLegalMoveSum =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> whiteSideKumalakSumAtMove =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> blackSideKumalakSumAtMove =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> totalSideKumalakSumAtMove =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> whiteKazanSumAtMove =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  std::vector<uint64_t> blackKazanSumAtMove =
      std::vector<uint64_t>(TRACKED_MOVE_LIMIT + 1);
  uint64_t branchingExactMovePositionsOverflow = 0;
  uint64_t branchingExactMoveLegalMoveSumOverflow = 0;
  uint64_t whiteSideKumalakSumAtMoveOverflow = 0;
  uint64_t blackSideKumalakSumAtMoveOverflow = 0;
  uint64_t totalSideKumalakSumAtMoveOverflow = 0;
  uint64_t whiteKazanSumAtMoveOverflow = 0;
  uint64_t blackKazanSumAtMoveOverflow = 0;
  uint64_t max_capture_single_turn = 0;

  std::array<std::array<uint64_t, OUTCOME_COUNT>, 2>
      officialAtsyrauByPlayerOutcome{};
  std::array<uint64_t, 2> officialAtsyrauOpponentCaptureSum{};
  std::array<uint64_t, 2> officialAtsyrauFacedKazanSum{};
  std::array<uint64_t, 2> officialAtsyrauOpponentKazanSum{};
  std::array<uint64_t, 2> officialAtsyrauFacedLess81{};
  std::array<uint64_t, 2> officialAtsyrauFacedEqual81{};
  std::array<uint64_t, 2> officialAtsyrauFacedGreater81{};

  std::array<std::array<uint64_t, OUTCOME_COUNT>, 2>
      noLegalAfterScoreWinByPlayerOutcome{};
  std::array<std::array<uint64_t, OUTCOME_COUNT>, 3> terminalCauseOutcome{};
};

struct MoveEvents {
  int mover = PLAYER_1;
  uint64_t capture_by_mover = 0;
  int tuzdyk_created_owner = -1;
  int tuzdyk_created_pos = -1;
  int tuzdyk_created_move = 0;
  int tuzdyk_blocked_player = -1;
  std::array<std::array<uint64_t, KUMALAK_POS_COUNT>, 2> tuzdyk_yield{};
  int atsyrau_by_player = -1;
  int no_legal_after_score_win_by_player = -1;
  std::array<uint64_t, 2> atsyrau_gain{};
  std::array<int, 2> kazan_before_atsyrau{};
};

struct GameFeatures {
  int white_first_move = -1;
  int black_first_move = -1;
  int atsyrau_by_player = -1;
  int no_legal_after_score_win_by_player = -1;
  uint64_t atsyrau_opponent_capture = 0;
  int atsyrau_faced_kazan_before = 0;
  int atsyrau_opponent_kazan_before = 0;
  bool max_steps_terminal = false;
};

struct SplitMix64 {
  uint64_t state;

  explicit SplitMix64(uint64_t seed) : state(seed) {}

  // SplitMix64 is fast, deterministic, and cheap to split into independent
  // worker streams. That matters more here than cryptographic randomness.
  inline uint64_t next() {
    return splitmix64_mix(state += SPLITMIX_GAMMA);
  }

  inline int bounded(int bound) {
    return static_cast<int>(
        (static_cast<__uint128_t>(next()) * static_cast<uint64_t>(bound)) >>
        64);
  }
};

struct PrecomputedRng {
  std::vector<uint64_t> values;
  size_t idx = 0;

  PrecomputedRng(uint64_t seed, uint64_t num_games) {
    uint64_t wanted = std::max<uint64_t>(1024, num_games * 160);
    wanted = std::min<uint64_t>(wanted, 1ULL << 24);
    values.resize(static_cast<size_t>(wanted));
    SplitMix64 rng(seed);
    for (uint64_t i = 0; i < wanted; ++i) {
      values[static_cast<size_t>(i)] = rng.next();
    }
  }

  inline int bounded(int bound) {
    if (idx == values.size()) {
      idx = 0;
    }
    uint64_t raw = values[idx++];
    return static_cast<int>(
        (static_cast<__uint128_t>(raw) * static_cast<uint64_t>(bound)) >> 64);
  }
};

// Compact board used by the high-throughput simulator. It mirrors ToguzEnv's
// rule behavior but avoids virtual/heap-heavy paths during billion-game runs.
struct FastState {
  std::array<uint16_t, 18> board{};
  std::array<int, 2> kazans{};
  std::array<int, 2> kumalaks{};
  int to_play = PLAYER_1;
  int steps = 0;
  int winner_code = -2;

  inline void reset() {
    board.fill(INITIAL_STONES);
    kazans = {0, 0};
    kumalaks = {-1, -1};
    to_play = PLAYER_1;
    steps = 0;
    winner_code = -2;
  }
};

static bool has_prefix(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

static uint64_t parse_u64(const std::string &raw, const std::string &name) {
  size_t used = 0;
  uint64_t value = std::stoull(raw, &used);
  if (used != raw.size()) {
    throw std::runtime_error("Invalid value for " + name + ": " + raw);
  }
  return value;
}

static int parse_int(const std::string &raw, const std::string &name) {
  size_t used = 0;
  int value = std::stoi(raw, &used);
  if (used != raw.size()) {
    throw std::runtime_error("Invalid value for " + name + ": " + raw);
  }
  return value;
}

static fs::path default_stat_path(const char *argv0) {
  fs::path exe = argv0 == nullptr ? fs::path() : fs::path(argv0);
  fs::path dir = exe.has_parent_path() ? exe.parent_path() : fs::current_path();
  if (dir.empty()) {
    dir = fs::current_path();
  }
  return dir / "billion_game_statistics.txt";
}

static int default_thread_count() {
  unsigned int n = std::thread::hardware_concurrency();
  return n == 0 ? 1 : static_cast<int>(n);
}

static void print_help(const char *argv0) {
  std::cout << "Usage: " << argv0 << " --num=1000000 [options]\n"
            << "\n"
            << "Runs random-vs-random Togyz Kumalak games from the initial "
               "setup and\n"
            << "adds the results to a structured statistics file.\n"
            << "\n"
            << "Options:\n"
            << "  --num=N              Number of games.\n"
            << "  --seed=N             Deterministic seed.\n"
            << "  --threads=N          Worker threads for fast-parallel mode.\n"
            << "  --mode=MODE          env, env-fast-rng, fast, "
               "fast-prebuffer, fast-parallel.\n"
            << "  --bench[=N]          Compare modes over N games (default: "
               "--num value).\n"
            << "  --format-only        Reformat an existing stats file without "
               "adding games.\n"
            << "  --fresh              Ignore existing counters and write a "
               "fresh stats file.\n"
            << "  --max-steps=N        Safety move cap per game.\n"
            << "  --stat=PATH          Output stats file.\n";
}

static Args parse_args(int argc, char *argv[]) {
  Args args;
  args.seed = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  args.stat_path = default_stat_path(argc > 0 ? argv[0] : nullptr);
  args.threads = default_thread_count();

  auto require_value = [&](int &idx, const std::string &name) -> std::string {
    if (idx + 1 >= argc) {
      throw std::runtime_error("Missing value after " + name);
    }
    return argv[++idx];
  };

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      print_help(argv[0]);
      std::exit(0);
    } else if (has_prefix(arg, "--num=")) {
      args.num_games = parse_u64(arg.substr(6), "--num");
    } else if (arg == "--num" || arg == "--numgames" || arg == "--games") {
      args.num_games = parse_u64(require_value(i, arg), arg);
    } else if (has_prefix(arg, "--numgames=")) {
      args.num_games = parse_u64(arg.substr(11), "--numgames");
    } else if (has_prefix(arg, "--games=")) {
      args.num_games = parse_u64(arg.substr(8), "--games");
    } else if (has_prefix(arg, "--seed=")) {
      args.seed = parse_u64(arg.substr(7), "--seed");
    } else if (arg == "--seed") {
      args.seed = parse_u64(require_value(i, arg), arg);
    } else if (has_prefix(arg, "--threads=")) {
      args.threads = parse_int(arg.substr(10), "--threads");
    } else if (arg == "--threads") {
      args.threads = parse_int(require_value(i, arg), arg);
    } else if (has_prefix(arg, "--mode=")) {
      args.mode = arg.substr(7);
    } else if (arg == "--mode") {
      args.mode = require_value(i, arg);
    } else if (has_prefix(arg, "--max-steps=")) {
      args.max_steps = parse_int(arg.substr(12), "--max-steps");
    } else if (arg == "--max-steps" || arg == "--maxsteps") {
      args.max_steps = parse_int(require_value(i, arg), arg);
    } else if (has_prefix(arg, "--maxsteps=")) {
      args.max_steps = parse_int(arg.substr(11), "--maxsteps");
    } else if (has_prefix(arg, "--stat=")) {
      args.stat_path = arg.substr(7);
    } else if (arg == "--stat" || arg == "--output") {
      args.stat_path = require_value(i, arg);
    } else if (has_prefix(arg, "--output=")) {
      args.stat_path = arg.substr(9);
    } else if (arg == "--bench" || arg == "--benchmark") {
      args.benchmark = true;
    } else if (has_prefix(arg, "--bench=")) {
      args.benchmark = true;
      args.num_games = parse_u64(arg.substr(8), "--bench");
    } else if (has_prefix(arg, "--benchmark=")) {
      args.benchmark = true;
      args.num_games = parse_u64(arg.substr(12), "--benchmark");
    } else if (arg == "--format-only" || arg == "--reformat") {
      args.format_only = true;
    } else if (arg == "--fresh" || arg == "--reset") {
      args.fresh_output = true;
    } else {
      throw std::runtime_error("Unknown argument: " + arg);
    }
  }

  if (!args.format_only && args.num_games == 0) {
    throw std::runtime_error("--num must be >= 1");
  }
  if (args.max_steps < 1) {
    throw std::runtime_error("--max-steps must be >= 1");
  }
  if (args.threads < 1) {
    throw std::runtime_error("--threads must be >= 1");
  }
  return args;
}

static void merge_stats(Stats &dst, const Stats &src) {
  dst.games += src.games;
  dst.whitewin += src.whitewin;
  dst.draw += src.draw;
  dst.blackwin += src.blackwin;
  dst.withNoKumalaks += src.withNoKumalaks;
  dst.withWhiteKumalakONLY += src.withWhiteKumalakONLY;
  dst.withBlackKumalakONLY += src.withBlackKumalakONLY;
  dst.withBOTHKumalaks += src.withBOTHKumalaks;
  dst.moments_where_one_player_had_no_legal_move +=
      src.moments_where_one_player_had_no_legal_move;
  dst.moves_of_all_games += src.moves_of_all_games;
  dst.positions_observed += src.positions_observed;
  dst.sum_of_all_legal_moves_per_position +=
      src.sum_of_all_legal_moves_per_position;
  dst.max_step_games += src.max_step_games;
  dst.longest_game = std::max(dst.longest_game, src.longest_game);
  dst.feature_tracked_games += src.feature_tracked_games;
  dst.feature_move_sum += src.feature_move_sum;
  if (src.feature_min_moves != 0 &&
      (dst.feature_min_moves == 0 || src.feature_min_moves < dst.feature_min_moves)) {
    dst.feature_min_moves = src.feature_min_moves;
  }
  if (src.feature_min_moves_to_win != 0 &&
      (dst.feature_min_moves_to_win == 0 ||
       src.feature_min_moves_to_win < dst.feature_min_moves_to_win)) {
    dst.feature_min_moves_to_win = src.feature_min_moves_to_win;
  }
  dst.feature_max_moves = std::max(dst.feature_max_moves, src.feature_max_moves);
  dst.decisive_games += src.decisive_games;
  dst.sum_winning_score += src.sum_winning_score;
  dst.sum_losing_score += src.sum_losing_score;
  dst.close_wins_margin_0_to_5 += src.close_wins_margin_0_to_5;
  dst.blowout_wins_margin_50_plus += src.blowout_wins_margin_50_plus;
  dst.gameLengthMovesOverflow += src.gameLengthMovesOverflow;
  dst.exactMoveTrackedGames += src.exactMoveTrackedGames;
  dst.branchingExactMovePositionsOverflow +=
      src.branchingExactMovePositionsOverflow;
  dst.branchingExactMoveLegalMoveSumOverflow +=
      src.branchingExactMoveLegalMoveSumOverflow;
  dst.whiteSideKumalakSumAtMoveOverflow +=
      src.whiteSideKumalakSumAtMoveOverflow;
  dst.blackSideKumalakSumAtMoveOverflow +=
      src.blackSideKumalakSumAtMoveOverflow;
  dst.totalSideKumalakSumAtMoveOverflow +=
      src.totalSideKumalakSumAtMoveOverflow;
  dst.whiteKazanSumAtMoveOverflow += src.whiteKazanSumAtMoveOverflow;
  dst.blackKazanSumAtMoveOverflow += src.blackKazanSumAtMoveOverflow;
  dst.max_capture_single_turn =
      std::max(dst.max_capture_single_turn, src.max_capture_single_turn);

  for (size_t i = 0; i < dst.kumalakAtPos.size(); ++i) {
    dst.kumalakAtPos[i] += src.kumalakAtPos[i];
  }
  for (size_t i = 0; i < dst.legalMoveAtPos.size(); ++i) {
    dst.legalMoveAtPos[i] += src.legalMoveAtPos[i];
  }
  for (size_t i = 0; i < dst.tuzdykStateOutcome.size(); ++i) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.tuzdykStateOutcome[i][outcome] += src.tuzdykStateOutcome[i][outcome];
    }
  }
  for (size_t i = 0; i < dst.whiteTuzdykPosOutcome.size(); ++i) {
    dst.whiteTuzdykYield[i] += src.whiteTuzdykYield[i];
    dst.blackTuzdykYield[i] += src.blackTuzdykYield[i];
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.whiteTuzdykPosOutcome[i][outcome] +=
          src.whiteTuzdykPosOutcome[i][outcome];
      dst.blackTuzdykPosOutcome[i][outcome] +=
          src.blackTuzdykPosOutcome[i][outcome];
    }
  }
  for (size_t player = 0; player < 2; ++player) {
    dst.tuzdykBlockedBySymmetry[player] += src.tuzdykBlockedBySymmetry[player];
    dst.tuzdykCreatedAtMoveOverflow[player] +=
        src.tuzdykCreatedAtMoveOverflow[player];
    dst.atsyrauOpponentCaptureSum[player] += src.atsyrauOpponentCaptureSum[player];
    dst.officialAtsyrauOpponentCaptureSum[player] +=
        src.officialAtsyrauOpponentCaptureSum[player];
    dst.officialAtsyrauFacedKazanSum[player] +=
        src.officialAtsyrauFacedKazanSum[player];
    dst.officialAtsyrauOpponentKazanSum[player] +=
        src.officialAtsyrauOpponentKazanSum[player];
    dst.officialAtsyrauFacedLess81[player] +=
        src.officialAtsyrauFacedLess81[player];
    dst.officialAtsyrauFacedEqual81[player] +=
        src.officialAtsyrauFacedEqual81[player];
    dst.officialAtsyrauFacedGreater81[player] +=
        src.officialAtsyrauFacedGreater81[player];
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.atsyrauByPlayerOutcome[player][outcome] +=
          src.atsyrauByPlayerOutcome[player][outcome];
      dst.officialAtsyrauByPlayerOutcome[player][outcome] +=
          src.officialAtsyrauByPlayerOutcome[player][outcome];
      dst.noLegalAfterScoreWinByPlayerOutcome[player][outcome] +=
          src.noLegalAfterScoreWinByPlayerOutcome[player][outcome];
    }
  }
  for (size_t cause = 0; cause < dst.terminalCauseOutcome.size(); ++cause) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.terminalCauseOutcome[cause][outcome] +=
          src.terminalCauseOutcome[cause][outcome];
    }
  }
  for (size_t i = 0; i < dst.whiteTuzdykCreatedAtMove.size(); ++i) {
    dst.whiteTuzdykCreatedAtMove[i] += src.whiteTuzdykCreatedAtMove[i];
    dst.blackTuzdykCreatedAtMove[i] += src.blackTuzdykCreatedAtMove[i];
    dst.gameLengthMoves[i] += src.gameLengthMoves[i];
    dst.branchingExactMovePositions[i] += src.branchingExactMovePositions[i];
    dst.branchingExactMoveLegalMoveSum[i] +=
        src.branchingExactMoveLegalMoveSum[i];
    dst.whiteSideKumalakSumAtMove[i] +=
        src.whiteSideKumalakSumAtMove[i];
    dst.blackSideKumalakSumAtMove[i] +=
        src.blackSideKumalakSumAtMove[i];
    dst.totalSideKumalakSumAtMove[i] +=
        src.totalSideKumalakSumAtMove[i];
    dst.whiteKazanSumAtMove[i] += src.whiteKazanSumAtMove[i];
    dst.blackKazanSumAtMove[i] += src.blackKazanSumAtMove[i];
  }
  for (size_t i = 0; i < dst.whiteFirstMoveOutcome.size(); ++i) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.whiteFirstMoveOutcome[i][outcome] +=
          src.whiteFirstMoveOutcome[i][outcome];
    }
  }
  for (size_t i = 0; i < dst.openingPairOutcome.size(); ++i) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      dst.openingPairOutcome[i][outcome] += src.openingPairOutcome[i][outcome];
    }
  }
  for (size_t i = 0; i < dst.winMargin.size(); ++i) {
    dst.winMargin[i] += src.winMargin[i];
  }
  for (size_t i = 0; i < BRANCHING_BAND_COUNT; ++i) {
    dst.branchingPositions[i] += src.branchingPositions[i];
    dst.branchingLegalMoveSum[i] += src.branchingLegalMoveSum[i];
  }
}

static bool parse_counter_value(const std::string &raw, uint64_t &out) {
  try {
    size_t used = 0;
    out = std::stoull(raw, &used);
    return used == raw.size();
  } catch (...) {
    return false;
  }
}

static bool parse_indexed_counter(const std::string &key,
                                  const std::string &prefix, size_t max_size,
                                  size_t &idx) {
  if (!has_prefix(key, prefix)) {
    return false;
  }
  std::string raw_idx = key.substr(prefix.size());
  if (raw_idx.empty()) {
    return false;
  }

  try {
    size_t used = 0;
    uint64_t parsed = std::stoull(raw_idx, &used);
    if (used != raw_idx.size() || parsed >= max_size) {
      return false;
    }
    idx = static_cast<size_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

static int outcome_from_name(const std::string &name) {
  if (name == "whitewin") {
    return OUTCOME_WHITEWIN;
  }
  if (name == "draw") {
    return OUTCOME_DRAW;
  }
  if (name == "blackwin") {
    return OUTCOME_BLACKWIN;
  }
  return -1;
}

static const char *outcome_name(size_t outcome) {
  static const std::array<const char *, OUTCOME_COUNT> names = {
      "whitewin", "draw", "blackwin"};
  return names[outcome];
}

static int outcome_from_winner(int winner_code) {
  if (winner_code == PLAYER_1) {
    return OUTCOME_WHITEWIN;
  }
  if (winner_code == PLAYER_2) {
    return OUTCOME_BLACKWIN;
  }
  return OUTCOME_DRAW;
}

static bool parse_indexed_outcome_counter(const std::string &key,
                                          const std::string &prefix,
                                          size_t max_size, size_t &idx,
                                          int &outcome) {
  if (!has_prefix(key, prefix)) {
    return false;
  }

  std::string rest = key.substr(prefix.size());
  size_t sep = rest.find('_');
  if (sep == std::string::npos || sep == 0 || sep + 1 >= rest.size()) {
    return false;
  }

  std::string raw_idx = rest.substr(0, sep);
  std::string raw_outcome = rest.substr(sep + 1);
  try {
    size_t used = 0;
    uint64_t parsed = std::stoull(raw_idx, &used);
    int parsed_outcome = outcome_from_name(raw_outcome);
    if (used != raw_idx.size() || parsed >= max_size || parsed_outcome == -1) {
      return false;
    }
    idx = static_cast<size_t>(parsed);
    outcome = parsed_outcome;
    return true;
  } catch (...) {
    return false;
  }
}

static void set_loaded_counter(Stats &stats, const std::string &key,
                               uint64_t value) {
  if (key == "%games") {
    stats.games = value;
  } else if (key == "%whitewin") {
    stats.whitewin = value;
  } else if (key == "%draw") {
    stats.draw = value;
  } else if (key == "%blackwin") {
    stats.blackwin = value;
  } else if (key == "%withNoKumalaks") {
    stats.withNoKumalaks = value;
  } else if (key == "%withWhiteKumalakONLY") {
    stats.withWhiteKumalakONLY = value;
  } else if (key == "%withBlackKumalakONLY") {
    stats.withBlackKumalakONLY = value;
  } else if (key == "%withBOTHKumalaks") {
    stats.withBOTHKumalaks = value;
  } else if (key == "%moments_where_one_player_had_no_legal_move") {
    stats.moments_where_one_player_had_no_legal_move = value;
  } else if (key == "%moves_of_all_games") {
    stats.moves_of_all_games = value;
  } else if (key == "%positions_observed") {
    stats.positions_observed = value;
  } else if (key == "%sum_of_all_legal_moves_per_position") {
    stats.sum_of_all_legal_moves_per_position = value;
  } else if (key == "%maxStepGames") {
    stats.max_step_games = value;
  } else if (key == "%longestGameMoves") {
    stats.longest_game = value;
  } else if (key == "%featureTrackedGames") {
    stats.feature_tracked_games = value;
  } else if (key == "%featureMoveSum") {
    stats.feature_move_sum = value;
  } else if (key == "%featureMinMoves") {
    stats.feature_min_moves = value;
  } else if (key == "%featureMinMovesToWin") {
    stats.feature_min_moves_to_win = value;
  } else if (key == "%featureMaxMoves") {
    stats.feature_max_moves = value;
  } else if (key == "%exactMoveTrackedGames") {
    stats.exactMoveTrackedGames = value;
  } else if (key == "%decisiveGames") {
    stats.decisive_games = value;
  } else if (key == "%sumWinningScore") {
    stats.sum_winning_score = value;
  } else if (key == "%sumLosingScore") {
    stats.sum_losing_score = value;
  } else if (key == "%closeWinsMargin0to5") {
    stats.close_wins_margin_0_to_5 = value;
  } else if (key == "%blowoutWinsMargin50plus") {
    stats.blowout_wins_margin_50_plus = value;
  } else if (key == "%gameLengthMovesOverflow") {
    stats.gameLengthMovesOverflow = value;
  } else if (key == "%branchingExactMovePositionsOverflow") {
    stats.branchingExactMovePositionsOverflow = value;
  } else if (key == "%branchingExactMoveLegalMoveSumOverflow") {
    stats.branchingExactMoveLegalMoveSumOverflow = value;
  } else if (key == "%whiteSideKumalakSumAtMoveOverflow") {
    stats.whiteSideKumalakSumAtMoveOverflow = value;
  } else if (key == "%blackSideKumalakSumAtMoveOverflow") {
    stats.blackSideKumalakSumAtMoveOverflow = value;
  } else if (key == "%totalSideKumalakSumAtMoveOverflow") {
    stats.totalSideKumalakSumAtMoveOverflow = value;
  } else if (key == "%whiteKazanSumAtMoveOverflow") {
    stats.whiteKazanSumAtMoveOverflow = value;
  } else if (key == "%blackKazanSumAtMoveOverflow") {
    stats.blackKazanSumAtMoveOverflow = value;
  } else if (key == "%maxCaptureSingleTurn") {
    stats.max_capture_single_turn = value;
  } else {
    size_t idx = 0;
    int outcome = -1;
    if (parse_indexed_counter(key, "%kumalakAtPos", stats.kumalakAtPos.size(),
                              idx)) {
      stats.kumalakAtPos[idx] = value;
    } else if (parse_indexed_counter(key, "%legalMoveAtPos",
                                     stats.legalMoveAtPos.size(), idx)) {
      stats.legalMoveAtPos[idx] = value;
    } else if (parse_indexed_outcome_counter(key, "%tuzdykState",
                                             stats.tuzdykStateOutcome.size(),
                                             idx, outcome)) {
      stats.tuzdykStateOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%whiteTuzdykPos", stats.whiteTuzdykPosOutcome.size(),
                   idx, outcome)) {
      stats.whiteTuzdykPosOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%blackTuzdykPos", stats.blackTuzdykPosOutcome.size(),
                   idx, outcome)) {
      stats.blackTuzdykPosOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_counter(key, "%whiteTuzdykYieldPos",
                                     stats.whiteTuzdykYield.size(), idx)) {
      stats.whiteTuzdykYield[idx] = value;
    } else if (parse_indexed_counter(key, "%blackTuzdykYieldPos",
                                     stats.blackTuzdykYield.size(), idx)) {
      stats.blackTuzdykYield[idx] = value;
    } else if (parse_indexed_counter(key, "%tuzdykBlockedBySymmetryPlayer",
                                     stats.tuzdykBlockedBySymmetry.size(), idx)) {
      stats.tuzdykBlockedBySymmetry[idx] = value;
    } else if (parse_indexed_counter(key, "%whiteTuzdykCreatedAtMove",
                                     stats.whiteTuzdykCreatedAtMove.size(),
                                     idx)) {
      stats.whiteTuzdykCreatedAtMove[idx] = value;
    } else if (parse_indexed_counter(key, "%blackTuzdykCreatedAtMove",
                                     stats.blackTuzdykCreatedAtMove.size(),
                                     idx)) {
      stats.blackTuzdykCreatedAtMove[idx] = value;
    } else if (parse_indexed_counter(key, "%tuzdykCreatedAtMoveOverflowPlayer",
                                     stats.tuzdykCreatedAtMoveOverflow.size(),
                                     idx)) {
      stats.tuzdykCreatedAtMoveOverflow[idx] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%whiteFirstMovePos",
                   stats.whiteFirstMoveOutcome.size(), idx, outcome)) {
      stats.whiteFirstMoveOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%openingPair", stats.openingPairOutcome.size(), idx,
                   outcome)) {
      stats.openingPairOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%atsyrauByPlayer",
                   stats.atsyrauByPlayerOutcome.size(), idx, outcome)) {
      stats.atsyrauByPlayerOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_counter(key, "%atsyrauOpponentCaptureByPlayer",
                                     stats.atsyrauOpponentCaptureSum.size(),
                                     idx)) {
      stats.atsyrauOpponentCaptureSum[idx] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%officialAtsyrauByPlayer",
                   stats.officialAtsyrauByPlayerOutcome.size(), idx,
                   outcome)) {
      stats.officialAtsyrauByPlayerOutcome[idx][static_cast<size_t>(outcome)] =
          value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauOpponentCaptureByPlayer",
                   stats.officialAtsyrauOpponentCaptureSum.size(), idx)) {
      stats.officialAtsyrauOpponentCaptureSum[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauFacedKazanSumByPlayer",
                   stats.officialAtsyrauFacedKazanSum.size(), idx)) {
      stats.officialAtsyrauFacedKazanSum[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauOpponentKazanSumByPlayer",
                   stats.officialAtsyrauOpponentKazanSum.size(), idx)) {
      stats.officialAtsyrauOpponentKazanSum[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauFacedLess81ByPlayer",
                   stats.officialAtsyrauFacedLess81.size(), idx)) {
      stats.officialAtsyrauFacedLess81[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauFacedEqual81ByPlayer",
                   stats.officialAtsyrauFacedEqual81.size(), idx)) {
      stats.officialAtsyrauFacedEqual81[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%officialAtsyrauFacedGreater81ByPlayer",
                   stats.officialAtsyrauFacedGreater81.size(), idx)) {
      stats.officialAtsyrauFacedGreater81[idx] = value;
    } else if (parse_indexed_outcome_counter(
                   key, "%noLegalAfterScoreWinByPlayer",
                   stats.noLegalAfterScoreWinByPlayerOutcome.size(), idx,
                   outcome)) {
      stats.noLegalAfterScoreWinByPlayerOutcome[idx]
                                             [static_cast<size_t>(outcome)] =
          value;
    } else if (parse_indexed_outcome_counter(
                   key, "%terminalCause", stats.terminalCauseOutcome.size(),
                   idx, outcome)) {
      stats.terminalCauseOutcome[idx][static_cast<size_t>(outcome)] = value;
    } else if (parse_indexed_counter(key, "%winMargin", stats.winMargin.size(),
                                     idx)) {
      stats.winMargin[idx] = value;
    } else if (parse_indexed_counter(key, "%gameLengthMoves",
                                     stats.gameLengthMoves.size(), idx)) {
      stats.gameLengthMoves[idx] = value;
    } else if (parse_indexed_counter(key, "%branchingExactMovePositions",
                                     stats.branchingExactMovePositions.size(),
                                     idx)) {
      stats.branchingExactMovePositions[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%branchingExactMoveLegalMoveSum",
                   stats.branchingExactMoveLegalMoveSum.size(), idx)) {
      stats.branchingExactMoveLegalMoveSum[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%whiteSideKumalakSumAtMove",
                   stats.whiteSideKumalakSumAtMove.size(), idx)) {
      stats.whiteSideKumalakSumAtMove[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%blackSideKumalakSumAtMove",
                   stats.blackSideKumalakSumAtMove.size(), idx)) {
      stats.blackSideKumalakSumAtMove[idx] = value;
    } else if (parse_indexed_counter(
                   key, "%totalSideKumalakSumAtMove",
                   stats.totalSideKumalakSumAtMove.size(), idx)) {
      stats.totalSideKumalakSumAtMove[idx] = value;
    } else if (parse_indexed_counter(key, "%whiteKazanSumAtMove",
                                     stats.whiteKazanSumAtMove.size(), idx)) {
      stats.whiteKazanSumAtMove[idx] = value;
    } else if (parse_indexed_counter(key, "%blackKazanSumAtMove",
                                     stats.blackKazanSumAtMove.size(), idx)) {
      stats.blackKazanSumAtMove[idx] = value;
    } else if (parse_indexed_counter(key, "%branchingBandPositions",
                                     stats.branchingPositions.size(), idx)) {
      stats.branchingPositions[idx] = value;
    } else if (parse_indexed_counter(key, "%branchingBandLegalMoveSum",
                                     stats.branchingLegalMoveSum.size(), idx)) {
      stats.branchingLegalMoveSum[idx] = value;
    }
  }
}

static Stats read_existing_stats(const fs::path &path) {
  Stats stats;
  if (!fs::exists(path) || fs::is_empty(path)) {
    return stats;
  }

  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Could not read stats file: " + path.string());
  }

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    std::string key;
    std::string raw_value;
    if (!(row >> key >> raw_value)) {
      continue;
    }
    if (key.empty() || key[0] != '%') {
      continue;
    }

    uint64_t value = 0;
    if (!parse_counter_value(raw_value, value)) {
      continue;
    }

    set_loaded_counter(stats, key, value);
  }

  return stats;
}

static inline void determine_winner(int kazans0, int kazans1,
                                    int &winner_code) {
  if (kazans0 > kazans1) {
    winner_code = PLAYER_1;
  } else if (kazans1 > kazans0) {
    winner_code = PLAYER_2;
  } else {
    winner_code = -1;
  }
}

static int collect_legal(ToguzEnv &env, std::array<int, 9> &moves) {
  std::array<int, 9> actions{};
  env.update_legal_actions(actions);

  int count = 0;
  for (int i = 0; i < NUM_PITS; ++i) {
    if (actions[i]) {
      moves[count++] = i;
    }
  }
  return count;
}

static inline int collect_legal_fast(const FastState &st,
                                     std::array<int, 9> &moves) {
  const int base = st.to_play * NUM_PITS;
  const int blocked = st.kumalaks[1 - st.to_play];
  int count = 0;

  for (int i = 0; i < NUM_PITS; ++i) {
    if (i != blocked && st.board[base + i] > 0) {
      moves[count++] = i;
    }
  }
  return count;
}

static inline bool has_legal_fast(const FastState &st) {
  const int base = st.to_play * NUM_PITS;
  const int blocked = st.kumalaks[1 - st.to_play];
  for (int i = 0; i < NUM_PITS; ++i) {
    if (i != blocked && st.board[base + i] > 0) {
      return true;
    }
  }
  return false;
}

static inline int branching_band_for_ply(int ply) {
  if (ply <= 20) {
    return 0;
  }
  if (ply <= 60) {
    return 1;
  }
  return 2;
}

static inline void add_position_stats(Stats &stats,
                                      const std::array<int, 9> &moves,
                                      int count, int ply,
                                      bool track_branching = false) {
  stats.positions_observed++;
  stats.sum_of_all_legal_moves_per_position += static_cast<uint64_t>(count);
  for (int i = 0; i < count; ++i) {
    stats.legalMoveAtPos[static_cast<size_t>(moves[i])]++;
  }

  if (!track_branching) {
    return;
  }

  int band = branching_band_for_ply(ply);
  stats.branchingPositions[static_cast<size_t>(band)]++;
  stats.branchingLegalMoveSum[static_cast<size_t>(band)] +=
      static_cast<uint64_t>(count);
}

static inline void add_exact_move_stats(Stats &stats, int ply, int legal_count,
                                        int white_side_kumalaks,
                                        int black_side_kumalaks,
                                        int white_kazan,
                                        int black_kazan) {
  const uint64_t white_side =
      static_cast<uint64_t>(std::max(0, white_side_kumalaks));
  const uint64_t black_side =
      static_cast<uint64_t>(std::max(0, black_side_kumalaks));
  const uint64_t total_side = white_side + black_side;
  const uint64_t white_store = static_cast<uint64_t>(std::max(0, white_kazan));
  const uint64_t black_store = static_cast<uint64_t>(std::max(0, black_kazan));

  if (ply >= 0 && static_cast<size_t>(ply) <= TRACKED_MOVE_LIMIT) {
    size_t idx = static_cast<size_t>(ply);
    stats.branchingExactMovePositions[idx]++;
    stats.branchingExactMoveLegalMoveSum[idx] +=
        static_cast<uint64_t>(legal_count);
    stats.whiteSideKumalakSumAtMove[idx] += white_side;
    stats.blackSideKumalakSumAtMove[idx] += black_side;
    stats.totalSideKumalakSumAtMove[idx] += total_side;
    stats.whiteKazanSumAtMove[idx] += white_store;
    stats.blackKazanSumAtMove[idx] += black_store;
    return;
  }

  stats.branchingExactMovePositionsOverflow++;
  stats.branchingExactMoveLegalMoveSumOverflow +=
      static_cast<uint64_t>(legal_count);
  stats.whiteSideKumalakSumAtMoveOverflow += white_side;
  stats.blackSideKumalakSumAtMoveOverflow += black_side;
  stats.totalSideKumalakSumAtMoveOverflow += total_side;
  stats.whiteKazanSumAtMoveOverflow += white_store;
  stats.blackKazanSumAtMoveOverflow += black_store;
}

static inline void side_kumalak_counts(const Bitboard &board, int &white_side,
                                       int &black_side) {
  white_side = 0;
  black_side = 0;
  for (int i = 0; i < NUM_PITS; ++i) {
    white_side += board.get(i);
    black_side += board.get(i + NUM_PITS);
  }
}

static inline void side_kumalak_counts(const FastState &st, int &white_side,
                                       int &black_side) {
  white_side = 0;
  black_side = 0;
  for (int i = 0; i < NUM_PITS; ++i) {
    white_side += st.board[static_cast<size_t>(i)];
    black_side += st.board[static_cast<size_t>(i + NUM_PITS)];
  }
}

static void add_final_game_stats(const ToguzEnv &env, Stats &stats) {
  stats.games++;
  stats.exactMoveTrackedGames++;
  stats.moves_of_all_games += static_cast<uint64_t>(env.steps);
  stats.longest_game =
      std::max<uint64_t>(stats.longest_game, static_cast<uint64_t>(env.steps));

  if (env.winner_code == PLAYER_1) {
    stats.whitewin++;
  } else if (env.winner_code == PLAYER_2) {
    stats.blackwin++;
  } else {
    stats.draw++;
  }

  const bool white_has = env.tuzduks[PLAYER_1] != -1;
  const bool black_has = env.tuzduks[PLAYER_2] != -1;

  if (!white_has && !black_has) {
    stats.withNoKumalaks++;
  } else if (white_has && !black_has) {
    stats.withWhiteKumalakONLY++;
  } else if (!white_has && black_has) {
    stats.withBlackKumalakONLY++;
  } else {
    stats.withBOTHKumalaks++;
  }

  for (int player = 0; player < 2; ++player) {
    int pos = env.tuzduks[player];
    if (pos >= 0) {
      stats.kumalakAtPos[static_cast<size_t>(pos)]++;
    }
  }
}

static void add_final_game_stats(const FastState &st, Stats &stats) {
  stats.games++;
  stats.exactMoveTrackedGames++;
  stats.moves_of_all_games += static_cast<uint64_t>(st.steps);
  stats.longest_game =
      std::max<uint64_t>(stats.longest_game, static_cast<uint64_t>(st.steps));

  if (st.winner_code == PLAYER_1) {
    stats.whitewin++;
  } else if (st.winner_code == PLAYER_2) {
    stats.blackwin++;
  } else {
    stats.draw++;
  }

  const bool white_has = st.kumalaks[PLAYER_1] != -1;
  const bool black_has = st.kumalaks[PLAYER_2] != -1;

  if (!white_has && !black_has) {
    stats.withNoKumalaks++;
  } else if (white_has && !black_has) {
    stats.withWhiteKumalakONLY++;
  } else if (!white_has && black_has) {
    stats.withBlackKumalakONLY++;
  } else {
    stats.withBOTHKumalaks++;
  }

  for (int player = 0; player < 2; ++player) {
    int pos = st.kumalaks[player];
    if (pos >= 0) {
      stats.kumalakAtPos[static_cast<size_t>(pos)]++;
    }
  }
}

static inline void step_fast(FastState &st, int action, MoveEvents &events) {
  events = MoveEvents();

  const int p = st.to_play;
  const int opp = 1 - p;
  const int start = p * NUM_PITS + action;
  const int stones = st.board[static_cast<size_t>(start)];
  const int kazan_before_mover = st.kazans[p];
  events.mover = p;
  int last = start;

  if (stones == 1) {
    st.board[static_cast<size_t>(start)] = 0;
    last = (start + 1) % 18;
    st.board[static_cast<size_t>(last)]++;
  } else {
    st.board[static_cast<size_t>(start)] = 1;
    const int sow_count = stones - 1;
    const int cycles = sow_count / 18;
    const int rem = sow_count % 18;
    if (cycles != 0) {
      for (uint16_t &pit : st.board) {
        pit = static_cast<uint16_t>(pit + cycles);
      }
    }
    for (int i = 0; i < rem; ++i) {
      int pit = (start + 1 + i) % 18;
      st.board[static_cast<size_t>(pit)]++;
    }
    last = (start + sow_count) % 18;
  }

  for (int owner = 0; owner < 2; ++owner) {
    const int pos = st.kumalaks[owner];
    if (pos != -1) {
      const int pit = (1 - owner) * NUM_PITS + pos;
      const int captured = st.board[static_cast<size_t>(pit)];
      if (captured > 0) {
        st.kazans[owner] += captured;
        st.board[static_cast<size_t>(pit)] = 0;
        if (pos >= 0 && pos < KUMALAK_POS_COUNT) {
          events.tuzdyk_yield[static_cast<size_t>(owner)]
                             [static_cast<size_t>(pos)] +=
              static_cast<uint64_t>(captured);
        }
      }
    }
  }

  if (last / NUM_PITS == opp) {
    const int col = last % NUM_PITS;
    const int val = st.board[static_cast<size_t>(last)];
    if (val == 3 && st.kumalaks[p] == -1 && col != 8) {
      if (st.kumalaks[opp] == col) {
        events.tuzdyk_blocked_player = p;
      } else {
        st.kumalaks[p] = col;
        st.kazans[p] += 3;
        st.board[static_cast<size_t>(last)] = 0;
        events.tuzdyk_created_owner = p;
        events.tuzdyk_created_pos = col;
        events.tuzdyk_created_move = st.steps + 1;
      }
    } else if (val % 2 == 0) {
      st.kazans[p] += val;
      st.board[static_cast<size_t>(last)] = 0;
    }
  }

  st.steps++;
  st.winner_code = -2;
  if (st.kazans[p] >= WIN_THRESHOLD) {
    st.winner_code = p;
  } else if (st.kazans[opp] >= WIN_THRESHOLD) {
    st.winner_code = opp;
  }

  st.to_play = opp;
  if (st.winner_code != -2) {
    if (!has_legal_fast(st)) {
      events.no_legal_after_score_win_by_player = st.to_play;
    }
  } else if (!has_legal_fast(st)) {
    events.atsyrau_by_player = st.to_play;
    events.kazan_before_atsyrau = st.kazans;
    for (int i = 0; i < NUM_PITS; ++i) {
      events.atsyrau_gain[PLAYER_1] += st.board[static_cast<size_t>(i)];
      events.atsyrau_gain[PLAYER_2] +=
          st.board[static_cast<size_t>(i + NUM_PITS)];
      st.kazans[PLAYER_1] += st.board[static_cast<size_t>(i)];
      st.kazans[PLAYER_2] += st.board[static_cast<size_t>(i + NUM_PITS)];
      st.board[static_cast<size_t>(i)] = 0;
      st.board[static_cast<size_t>(i + NUM_PITS)] = 0;
    }
    determine_winner(st.kazans[PLAYER_1], st.kazans[PLAYER_2], st.winner_code);
  }

  events.capture_by_mover =
      static_cast<uint64_t>(std::max(0, st.kazans[p] - kazan_before_mover));
}

static void add_move_events(Stats &stats, GameFeatures &game,
                            const MoveEvents &events) {
  stats.max_capture_single_turn =
      std::max(stats.max_capture_single_turn, events.capture_by_mover);

  for (int player = 0; player < 2; ++player) {
    for (int pos = 0; pos < KUMALAK_POS_COUNT; ++pos) {
      uint64_t captured =
          events.tuzdyk_yield[static_cast<size_t>(player)]
                             [static_cast<size_t>(pos)];
      if (captured == 0) {
        continue;
      }
      if (player == PLAYER_1) {
        stats.whiteTuzdykYield[static_cast<size_t>(pos)] += captured;
      } else {
        stats.blackTuzdykYield[static_cast<size_t>(pos)] += captured;
      }
    }
  }

  if (events.tuzdyk_created_owner != -1) {
    size_t move_idx = static_cast<size_t>(events.tuzdyk_created_move);
    if (move_idx <= TRACKED_MOVE_LIMIT) {
      if (events.tuzdyk_created_owner == PLAYER_1) {
        stats.whiteTuzdykCreatedAtMove[move_idx]++;
      } else {
        stats.blackTuzdykCreatedAtMove[move_idx]++;
      }
    } else {
      stats.tuzdykCreatedAtMoveOverflow
          [static_cast<size_t>(events.tuzdyk_created_owner)]++;
    }
  }

  if (events.tuzdyk_blocked_player != -1) {
    stats.tuzdykBlockedBySymmetry
        [static_cast<size_t>(events.tuzdyk_blocked_player)]++;
  }

  if (events.atsyrau_by_player != -1) {
    game.atsyrau_by_player = events.atsyrau_by_player;
    int opponent = 1 - events.atsyrau_by_player;
    game.atsyrau_opponent_capture =
        events.atsyrau_gain[static_cast<size_t>(opponent)];
    game.atsyrau_faced_kazan_before =
        events.kazan_before_atsyrau[static_cast<size_t>(events.atsyrau_by_player)];
    game.atsyrau_opponent_kazan_before =
        events.kazan_before_atsyrau[static_cast<size_t>(opponent)];
  }

  if (events.no_legal_after_score_win_by_player != -1) {
    game.no_legal_after_score_win_by_player =
        events.no_legal_after_score_win_by_player;
  }
}

static int tuzdyk_state_index(const FastState &st) {
  bool white_has = st.kumalaks[PLAYER_1] != -1;
  bool black_has = st.kumalaks[PLAYER_2] != -1;
  if (!white_has && !black_has) {
    return TUZDYK_STATE_NONE;
  }
  if (white_has && !black_has) {
    return TUZDYK_STATE_ONLY_WHITE;
  }
  if (!white_has && black_has) {
    return TUZDYK_STATE_ONLY_BLACK;
  }
  return TUZDYK_STATE_BOTH;
}

static void add_advanced_final_game_stats(const FastState &st,
                                          const GameFeatures &game,
                                          Stats &stats) {
  int outcome = outcome_from_winner(st.winner_code);
  stats.feature_tracked_games++;
  stats.feature_move_sum += static_cast<uint64_t>(st.steps);
  if (stats.feature_min_moves == 0 ||
      static_cast<uint64_t>(st.steps) < stats.feature_min_moves) {
    stats.feature_min_moves = static_cast<uint64_t>(st.steps);
  }
  stats.feature_max_moves =
      std::max<uint64_t>(stats.feature_max_moves, static_cast<uint64_t>(st.steps));

  if (st.steps <= TRACKED_MOVE_LIMIT) {
    stats.gameLengthMoves[static_cast<size_t>(st.steps)]++;
  } else {
    stats.gameLengthMovesOverflow++;
  }

  stats.tuzdykStateOutcome[static_cast<size_t>(tuzdyk_state_index(st))]
                          [static_cast<size_t>(outcome)]++;

  int white_pos = st.kumalaks[PLAYER_1];
  int black_pos = st.kumalaks[PLAYER_2];
  if (white_pos >= 0 && white_pos < KUMALAK_POS_COUNT) {
    stats.whiteTuzdykPosOutcome[static_cast<size_t>(white_pos)]
                               [static_cast<size_t>(outcome)]++;
  }
  if (black_pos >= 0 && black_pos < KUMALAK_POS_COUNT) {
    stats.blackTuzdykPosOutcome[static_cast<size_t>(black_pos)]
                               [static_cast<size_t>(outcome)]++;
  }

  if (game.white_first_move >= 0) {
    stats.whiteFirstMoveOutcome[static_cast<size_t>(game.white_first_move)]
                               [static_cast<size_t>(outcome)]++;
  }
  if (game.white_first_move >= 0 && game.black_first_move >= 0) {
    size_t opening_idx =
        static_cast<size_t>(game.white_first_move * LEGAL_POS_COUNT +
                            game.black_first_move);
    stats.openingPairOutcome[opening_idx][static_cast<size_t>(outcome)]++;
  }

  if (game.atsyrau_by_player != -1) {
    size_t player = static_cast<size_t>(game.atsyrau_by_player);
    stats.atsyrauByPlayerOutcome[player][static_cast<size_t>(outcome)]++;
    stats.atsyrauOpponentCaptureSum[player] += game.atsyrau_opponent_capture;
    stats.officialAtsyrauByPlayerOutcome[player][static_cast<size_t>(outcome)]++;
    stats.officialAtsyrauOpponentCaptureSum[player] +=
        game.atsyrau_opponent_capture;
    stats.officialAtsyrauFacedKazanSum[player] +=
        static_cast<uint64_t>(std::max(0, game.atsyrau_faced_kazan_before));
    stats.officialAtsyrauOpponentKazanSum[player] +=
        static_cast<uint64_t>(std::max(0, game.atsyrau_opponent_kazan_before));
    if (game.atsyrau_faced_kazan_before < 81) {
      stats.officialAtsyrauFacedLess81[player]++;
    } else if (game.atsyrau_faced_kazan_before == 81) {
      stats.officialAtsyrauFacedEqual81[player]++;
    } else {
      stats.officialAtsyrauFacedGreater81[player]++;
    }
  }

  if (game.no_legal_after_score_win_by_player != -1) {
    size_t player = static_cast<size_t>(game.no_legal_after_score_win_by_player);
    stats.noLegalAfterScoreWinByPlayerOutcome[player]
                                            [static_cast<size_t>(outcome)]++;
  }

  if (game.max_steps_terminal) {
    stats.terminalCauseOutcome[2][static_cast<size_t>(outcome)]++;
  } else if (game.atsyrau_by_player != -1) {
    stats.terminalCauseOutcome[1][static_cast<size_t>(outcome)]++;
  } else {
    stats.terminalCauseOutcome[0][static_cast<size_t>(outcome)]++;
  }

  if (st.winner_code != -1) {
    int winning_score = std::max(st.kazans[PLAYER_1], st.kazans[PLAYER_2]);
    int losing_score = std::min(st.kazans[PLAYER_1], st.kazans[PLAYER_2]);
    int margin = winning_score - losing_score;
    stats.decisive_games++;
    if (stats.feature_min_moves_to_win == 0 ||
        static_cast<uint64_t>(st.steps) < stats.feature_min_moves_to_win) {
      stats.feature_min_moves_to_win = static_cast<uint64_t>(st.steps);
    }
    stats.sum_winning_score += static_cast<uint64_t>(winning_score);
    stats.sum_losing_score += static_cast<uint64_t>(losing_score);
    if (margin <= 5) {
      stats.close_wins_margin_0_to_5++;
    }
    if (margin >= 50) {
      stats.blowout_wins_margin_50_plus++;
    }
    if (margin >= 0 && margin <= TOTAL_STONES) {
      stats.winMargin[static_cast<size_t>(margin)]++;
    }
  }
}

static Stats run_env_std(uint64_t games, uint64_t seed, int max_steps) {
  std::mt19937_64 rng(seed);
  ToguzEnv env;
  Stats stats;

  for (uint64_t game = 0; game < games; ++game) {
    env.reset();
    env.max_steps = max_steps;

    while (!env.is_game_over()) {
      std::array<int, 9> moves{};
      int count = collect_legal(env, moves);
      add_position_stats(stats, moves, count, env.steps + 1);
      int white_side = 0;
      int black_side = 0;
      side_kumalak_counts(env.board, white_side, black_side);
      add_exact_move_stats(stats, env.steps + 1, count, white_side, black_side,
                           env.kazans[PLAYER_1], env.kazans[PLAYER_2]);
      if (count == 0) {
        stats.moments_where_one_player_had_no_legal_move++;
        env.determine_winner_by_kazans();
        break;
      }

      std::uniform_int_distribution<int> dist(0, count - 1);
      env.step(moves[static_cast<size_t>(dist(rng))]);

      if (env.is_game_over()) {
        std::array<int, 9> next_moves{};
        int next_count = collect_legal(env, next_moves);
        if (next_count == 0) {
          stats.moments_where_one_player_had_no_legal_move++;
        }
      }
    }

    if (env.steps >= env.max_steps && env.winner_code == -2) {
      stats.max_step_games++;
      env.determine_winner_by_kazans();
    } else if (env.winner_code == -2) {
      env.determine_winner_by_kazans();
    }
    add_final_game_stats(env, stats);
  }
  return stats;
}

static Stats run_env_fast_rng(uint64_t games, uint64_t seed, int max_steps) {
  SplitMix64 rng(seed);
  ToguzEnv env;
  Stats stats;

  for (uint64_t game = 0; game < games; ++game) {
    env.reset();
    env.max_steps = max_steps;

    while (!env.is_game_over()) {
      std::array<int, 9> moves{};
      int count = collect_legal(env, moves);
      add_position_stats(stats, moves, count, env.steps + 1);
      int white_side = 0;
      int black_side = 0;
      side_kumalak_counts(env.board, white_side, black_side);
      add_exact_move_stats(stats, env.steps + 1, count, white_side, black_side,
                           env.kazans[PLAYER_1], env.kazans[PLAYER_2]);
      if (count == 0) {
        stats.moments_where_one_player_had_no_legal_move++;
        env.determine_winner_by_kazans();
        break;
      }

      env.step(moves[static_cast<size_t>(rng.bounded(count))]);

      if (env.is_game_over()) {
        std::array<int, 9> next_moves{};
        int next_count = collect_legal(env, next_moves);
        if (next_count == 0) {
          stats.moments_where_one_player_had_no_legal_move++;
        }
      }
    }

    if (env.steps >= env.max_steps && env.winner_code == -2) {
      stats.max_step_games++;
      env.determine_winner_by_kazans();
    } else if (env.winner_code == -2) {
      env.determine_winner_by_kazans();
    }
    add_final_game_stats(env, stats);
  }
  return stats;
}

template <typename Rng>
static Stats run_fast_core(uint64_t games, Rng &rng, int max_steps) {
  FastState st;
  Stats stats;

  for (uint64_t game = 0; game < games; ++game) {
    st.reset();
    GameFeatures game_features;

    while (st.winner_code == -2 && st.steps < max_steps) {
      std::array<int, 9> moves{};
      int count = collect_legal_fast(st, moves);
      add_position_stats(stats, moves, count, st.steps + 1, true);
      int white_side = 0;
      int black_side = 0;
      side_kumalak_counts(st, white_side, black_side);
      add_exact_move_stats(stats, st.steps + 1, count, white_side, black_side,
                           st.kazans[PLAYER_1], st.kazans[PLAYER_2]);

      if (count == 0) {
        stats.moments_where_one_player_had_no_legal_move++;
        determine_winner(st.kazans[PLAYER_1], st.kazans[PLAYER_2],
                         st.winner_code);
        break;
      }

      int move = moves[static_cast<size_t>(rng.bounded(count))];
      if (st.steps == 0) {
        game_features.white_first_move = move;
      } else if (st.steps == 1) {
        game_features.black_first_move = move;
      }

      MoveEvents events;
      step_fast(st, move, events);
      add_move_events(stats, game_features, events);
      if (events.atsyrau_by_player != -1) {
        stats.moments_where_one_player_had_no_legal_move++;
      }
    }

    if (st.steps >= max_steps && st.winner_code == -2) {
      stats.max_step_games++;
      game_features.max_steps_terminal = true;
      determine_winner(st.kazans[PLAYER_1], st.kazans[PLAYER_2],
                       st.winner_code);
    } else if (st.winner_code == -2) {
      determine_winner(st.kazans[PLAYER_1], st.kazans[PLAYER_2],
                       st.winner_code);
    }

    add_final_game_stats(st, stats);
    add_advanced_final_game_stats(st, game_features, stats);
  }

  return stats;
}

static Stats run_fast(uint64_t games, uint64_t seed, int max_steps) {
  SplitMix64 rng(seed);
  return run_fast_core(games, rng, max_steps);
}

static Stats run_fast_prebuffer(uint64_t games, uint64_t seed, int max_steps) {
  PrecomputedRng rng(seed, games);
  return run_fast_core(games, rng, max_steps);
}

static Stats run_fast_parallel(uint64_t games, uint64_t seed, int max_steps,
                               int threads) {
  threads = std::max(1, std::min<int>(threads, static_cast<int>(games)));
  std::vector<Stats> partial(static_cast<size_t>(threads));
  std::vector<std::thread> workers;
  workers.reserve(static_cast<size_t>(threads));

  uint64_t base = games / static_cast<uint64_t>(threads);
  uint64_t rem = games % static_cast<uint64_t>(threads);

  for (int t = 0; t < threads; ++t) {
    uint64_t count = base + (static_cast<uint64_t>(t) < rem ? 1 : 0);
    // Hash worker seeds so SplitMix streams are not one-step offsets.
    uint64_t thread_seed =
        splitmix64_mix(seed + SPLITMIX_GAMMA * static_cast<uint64_t>(t + 1));
    workers.emplace_back([&, t, count, thread_seed]() {
      partial[static_cast<size_t>(t)] = run_fast(count, thread_seed, max_steps);
    });
  }

  for (std::thread &worker : workers) {
    worker.join();
  }

  Stats total;
  for (const Stats &stats : partial) {
    merge_stats(total, stats);
  }
  return total;
}

static Stats run_mode(const Args &args) {
  // The slower modes are kept for validation and timing comparisons. The paper
  // run uses fast-parallel, which is the compact simulator split across CPU
  // threads.
  if (args.mode == "env") {
    return run_env_std(args.num_games, args.seed, args.max_steps);
  }
  if (args.mode == "env-fast-rng") {
    return run_env_fast_rng(args.num_games, args.seed, args.max_steps);
  }
  if (args.mode == "fast") {
    return run_fast(args.num_games, args.seed, args.max_steps);
  }
  if (args.mode == "fast-prebuffer") {
    return run_fast_prebuffer(args.num_games, args.seed, args.max_steps);
  }
  if (args.mode == "fast-parallel") {
    return run_fast_parallel(args.num_games, args.seed, args.max_steps,
                             args.threads);
  }
  throw std::runtime_error("Unknown mode: " + args.mode);
}

static uint64_t median_moves_from_hist(const Stats &stats) {
  if (stats.feature_tracked_games == 0) {
    return 0;
  }

  uint64_t target = (stats.feature_tracked_games + 1) / 2;
  uint64_t seen = 0;
  for (size_t i = 0; i < stats.gameLengthMoves.size(); ++i) {
    seen += stats.gameLengthMoves[i];
    if (seen >= target) {
      return static_cast<uint64_t>(i);
    }
  }
  return TRACKED_MOVE_LIMIT + 1;
}

static uint64_t avg_x1000(uint64_t sum, uint64_t count) {
  if (count == 0) {
    return 0;
  }
  return (sum * 1000ULL) / count;
}

static std::string with_commas(uint64_t value) {
  std::string s = std::to_string(value);
  for (int insert_at = static_cast<int>(s.size()) - 3; insert_at > 0;
       insert_at -= 3) {
    s.insert(static_cast<size_t>(insert_at), ",");
  }
  return s;
}

static std::string fixed_number(double value, int decimals) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(decimals) << value;
  return out.str();
}

static std::string percent(uint64_t part, uint64_t whole) {
  if (whole == 0) {
    return "0.00%";
  }
  return fixed_number(100.0 * static_cast<double>(part) /
                          static_cast<double>(whole),
                      2) +
         "%";
}

static double ratio(uint64_t numerator, uint64_t denominator) {
  if (denominator == 0) {
    return 0.0;
  }
  return static_cast<double>(numerator) / static_cast<double>(denominator);
}

static void write_human_summary(std::ostream &out, const Args &args,
                                const Stats &stats) {
  out << "# Togyzkumalak Billion-Game Statistics\n\n";
  out << "This file starts with a human-readable summary for the research "
         "paper, followed by stable machine-readable counters in `%key "
         "value` form.\n\n";

  out << "## Run Configuration\n\n";
  out << "| Field | Value |\n";
  out << "| :--- | ---: |\n";
  out << "| Games | " << with_commas(stats.games) << " |\n";
  out << "| Seed | " << args.seed << " |\n";
  out << "| Mode | " << args.mode << " |\n";
  out << "| Threads | "
      << (args.mode == "fast-parallel" ? args.threads : 1) << " |\n";
  out << "| Max steps per game | " << with_commas(args.max_steps) << " |\n";
  out << "| Max-step games | " << with_commas(stats.max_step_games) << " |\n\n";

  out << "## Headline Metrics\n\n";
  out << "| Metric | Value |\n";
  out << "| :--- | ---: |\n";
  out << "| Observed board positions | "
      << with_commas(stats.positions_observed) << " |\n";
  out << "| Total legal moves over observed positions | "
      << with_commas(stats.sum_of_all_legal_moves_per_position) << " |\n";
  out << "| Empirical average branching factor | "
      << fixed_number(ratio(stats.sum_of_all_legal_moves_per_position,
                            stats.positions_observed),
                      3)
      << " |\n";
  out << "| Average game length, halfmoves | "
      << fixed_number(ratio(stats.feature_move_sum,
                            stats.feature_tracked_games),
                      3)
      << " |\n";
  out << "| Median game length, halfmoves | "
      << with_commas(median_moves_from_hist(stats)) << " |\n";
  out << "| Shortest winning game observed | "
      << with_commas(stats.feature_min_moves_to_win) << " |\n";
  out << "| Longest random game observed | "
      << with_commas(stats.longest_game) << " |\n\n";

  out << "## Outcomes\n\n";
  out << "| Result | Games | Share |\n";
  out << "| :--- | ---: | ---: |\n";
  out << "| White / first player win | " << with_commas(stats.whitewin)
      << " | " << percent(stats.whitewin, stats.games) << " |\n";
  out << "| Draw | " << with_commas(stats.draw) << " | "
      << percent(stats.draw, stats.games) << " |\n";
  out << "| Black / second player win | " << with_commas(stats.blackwin)
      << " | " << percent(stats.blackwin, stats.games) << " |\n\n";

  out << "## Tuzdyk Ownership\n\n";
  out << "| State | Games | White win | Draw | Black win |\n";
  out << "| :--- | ---: | ---: | ---: | ---: |\n";
  const std::array<const char *, TUZDYK_STATE_COUNT> state_names = {
      "No Tuzdyks", "Only White has Tuzdyk", "Only Black has Tuzdyk",
      "Both players have Tuzdyks"};
  for (size_t state = 0; state < stats.tuzdykStateOutcome.size(); ++state) {
    const uint64_t total =
        stats.tuzdykStateOutcome[state][OUTCOME_WHITEWIN] +
        stats.tuzdykStateOutcome[state][OUTCOME_DRAW] +
        stats.tuzdykStateOutcome[state][OUTCOME_BLACKWIN];
    out << "| " << state_names[state] << " | " << with_commas(total) << " | "
        << percent(stats.tuzdykStateOutcome[state][OUTCOME_WHITEWIN], total)
        << " | " << percent(stats.tuzdykStateOutcome[state][OUTCOME_DRAW], total)
        << " | "
        << percent(stats.tuzdykStateOutcome[state][OUTCOME_BLACKWIN], total)
        << " |\n";
  }
  out << "\n";

  out << "## Branching Bands\n\n";
  out << "| Halfmove band | Observed positions | Average legal moves |\n";
  out << "| :--- | ---: | ---: |\n";
  const std::array<const char *, BRANCHING_BAND_COUNT> band_names = {
      "1..20", "21..60", "61+"};
  for (size_t band = 0; band < BRANCHING_BAND_COUNT; ++band) {
    out << "| " << band_names[band] << " | "
        << with_commas(stats.branchingPositions[band]) << " | "
        << fixed_number(ratio(stats.branchingLegalMoveSum[band],
                              stats.branchingPositions[band]),
                        3)
        << " |\n";
  }
  out << "\n";

  out << "## Machine-Readable Counters\n\n";
  out << "The counters below are raw counts. They are intentionally retained "
         "for scripts and exact reproduction checks.\n\n";
}

static void write_stats(const Args &args, const Stats &stats) {
  fs::path parent = args.stat_path.parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent);
  }

  std::ofstream out(args.stat_path);
  if (!out) {
    throw std::runtime_error("Could not write stats file: " +
                             args.stat_path.string());
  }

  write_human_summary(out, args, stats);

  out << "# Random model statistics for Togyz Kumalak\n";
  out << "# Counts are raw counts, not percentages.\n";
  out << "# White is PLAYER_1, the first player from the initial setup.\n";
  out << "# Kumalak counters use zero-based pit positions 0..7; legal move "
         "counters use 0..8.\n";
  out << "# Move-indexed counters use plies/half-moves: move 1 is White's "
         "first pit move, move 2 is Black's first pit move.\n";
  out << "# Existing counter values are loaded and summed before this file is "
         "rewritten.\n";
  out << "%games " << stats.games << "\n";
  out << "%seed " << args.seed << "\n";
  out << "%mode " << args.mode << "\n";
  out << "%threads " << (args.mode == "fast-parallel" ? args.threads : 1)
      << "\n";
  out << "%maxSteps " << args.max_steps << "\n";
  out << "%maxStepGames " << stats.max_step_games << "\n";
  out << "%longestGameMoves " << stats.longest_game << "\n";
  out << "\n";

  out << "%whitewin " << stats.whitewin << "\n";
  out << "%draw " << stats.draw << "\n";
  out << "%blackwin " << stats.blackwin << "\n";
  out << "\n";

  out << "%withNoKumalaks " << stats.withNoKumalaks << "\n";
  out << "%withWhiteKumalakONLY " << stats.withWhiteKumalakONLY << "\n";
  out << "%withBlackKumalakONLY " << stats.withBlackKumalakONLY << "\n";
  out << "%withBOTHKumalaks " << stats.withBOTHKumalaks << "\n";
  for (size_t i = 0; i < stats.kumalakAtPos.size(); ++i) {
    out << "%kumalakAtPos" << i << " " << stats.kumalakAtPos[i] << "\n";
  }
  out << "\n";

  out << "%moments_where_one_player_had_no_legal_move "
      << stats.moments_where_one_player_had_no_legal_move << "\n";
  out << "%moves_of_all_games " << stats.moves_of_all_games << "\n";
  out << "%positions_observed " << stats.positions_observed << "\n";
  out << "%sum_of_all_legal_moves_per_position "
      << stats.sum_of_all_legal_moves_per_position << "\n";
  for (size_t i = 0; i < stats.legalMoveAtPos.size(); ++i) {
    out << "%legalMoveAtPos" << i << " " << stats.legalMoveAtPos[i] << "\n";
  }
  out << "\n";

  out << "# Advanced feature counters. These start from zero for files created "
         "before the feature upgrade.\n";
  out << "%featureTrackedGames " << stats.feature_tracked_games << "\n";
  out << "%featureMoveSum " << stats.feature_move_sum << "\n";
  out << "%featureMinMoves " << stats.feature_min_moves << "\n";
  out << "%featureMinMovesToWin " << stats.feature_min_moves_to_win << "\n";
  out << "%featureMaxMoves " << stats.feature_max_moves << "\n";
  out << "%medianMoves " << median_moves_from_hist(stats) << "\n";
  out << "%averageMovesX1000 "
      << avg_x1000(stats.feature_move_sum, stats.feature_tracked_games) << "\n";
  out << "%decisiveGames " << stats.decisive_games << "\n";
  out << "%sumWinningScore " << stats.sum_winning_score << "\n";
  out << "%sumLosingScore " << stats.sum_losing_score << "\n";
  out << "%averageWinningScoreX1000 "
      << avg_x1000(stats.sum_winning_score, stats.decisive_games) << "\n";
  out << "%averageLosingScoreX1000 "
      << avg_x1000(stats.sum_losing_score, stats.decisive_games) << "\n";
  out << "%closeWinsMargin0to5 " << stats.close_wins_margin_0_to_5 << "\n";
  out << "%blowoutWinsMargin50plus " << stats.blowout_wins_margin_50_plus
      << "\n";
  out << "%maxCaptureSingleTurn " << stats.max_capture_single_turn << "\n";
  out << "\n";

  out << "# Tuzdyk state ids: 0=NONE, 1=ONLY_WHITE, 2=ONLY_BLACK, 3=BOTH.\n";
  for (size_t state = 0; state < stats.tuzdykStateOutcome.size(); ++state) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%tuzdykState" << state << "_" << outcome_name(outcome) << " "
          << stats.tuzdykStateOutcome[state][outcome] << "\n";
    }
  }
  for (size_t pos = 0; pos < stats.whiteTuzdykPosOutcome.size(); ++pos) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%whiteTuzdykPos" << pos << "_" << outcome_name(outcome) << " "
          << stats.whiteTuzdykPosOutcome[pos][outcome] << "\n";
      out << "%blackTuzdykPos" << pos << "_" << outcome_name(outcome) << " "
          << stats.blackTuzdykPosOutcome[pos][outcome] << "\n";
    }
    out << "%whiteTuzdykYieldPos" << pos << " " << stats.whiteTuzdykYield[pos]
        << "\n";
    out << "%blackTuzdykYieldPos" << pos << " " << stats.blackTuzdykYield[pos]
        << "\n";
  }
  for (size_t player = 0; player < 2; ++player) {
    out << "%tuzdykBlockedBySymmetryPlayer" << player << " "
        << stats.tuzdykBlockedBySymmetry[player] << "\n";
    out << "%tuzdykCreatedAtMoveOverflowPlayer" << player << " "
        << stats.tuzdykCreatedAtMoveOverflow[player] << "\n";
  }
  for (size_t move = 0; move < stats.whiteTuzdykCreatedAtMove.size(); ++move) {
    if (stats.whiteTuzdykCreatedAtMove[move] != 0) {
      out << "%whiteTuzdykCreatedAtMove" << move << " "
          << stats.whiteTuzdykCreatedAtMove[move] << "\n";
    }
    if (stats.blackTuzdykCreatedAtMove[move] != 0) {
      out << "%blackTuzdykCreatedAtMove" << move << " "
          << stats.blackTuzdykCreatedAtMove[move] << "\n";
    }
  }
  out << "\n";

  for (size_t pos = 0; pos < stats.whiteFirstMoveOutcome.size(); ++pos) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%whiteFirstMovePos" << pos << "_" << outcome_name(outcome) << " "
          << stats.whiteFirstMoveOutcome[pos][outcome] << "\n";
    }
  }
  out << "# openingPair index = white_first_move * 9 + black_first_move.\n";
  for (size_t idx = 0; idx < stats.openingPairOutcome.size(); ++idx) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%openingPair" << idx << "_" << outcome_name(outcome) << " "
          << stats.openingPairOutcome[idx][outcome] << "\n";
    }
  }
  out << "\n";

  for (size_t player = 0; player < 2; ++player) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%atsyrauByPlayer" << player << "_" << outcome_name(outcome)
          << " " << stats.atsyrauByPlayerOutcome[player][outcome] << "\n";
    }
    out << "%atsyrauOpponentCaptureByPlayer" << player << " "
        << stats.atsyrauOpponentCaptureSum[player] << "\n";
  }
  out << "# Official atsyrau excludes positions where the game was already "
         "won by 82+ before no-legal-move checking.\n";
  for (size_t player = 0; player < 2; ++player) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%officialAtsyrauByPlayer" << player << "_"
          << outcome_name(outcome) << " "
          << stats.officialAtsyrauByPlayerOutcome[player][outcome] << "\n";
      out << "%noLegalAfterScoreWinByPlayer" << player << "_"
          << outcome_name(outcome) << " "
          << stats.noLegalAfterScoreWinByPlayerOutcome[player][outcome]
          << "\n";
    }
    out << "%officialAtsyrauOpponentCaptureByPlayer" << player << " "
        << stats.officialAtsyrauOpponentCaptureSum[player] << "\n";
    out << "%officialAtsyrauFacedKazanSumByPlayer" << player << " "
        << stats.officialAtsyrauFacedKazanSum[player] << "\n";
    out << "%officialAtsyrauOpponentKazanSumByPlayer" << player << " "
        << stats.officialAtsyrauOpponentKazanSum[player] << "\n";
    out << "%officialAtsyrauFacedLess81ByPlayer" << player << " "
        << stats.officialAtsyrauFacedLess81[player] << "\n";
    out << "%officialAtsyrauFacedEqual81ByPlayer" << player << " "
        << stats.officialAtsyrauFacedEqual81[player] << "\n";
    out << "%officialAtsyrauFacedGreater81ByPlayer" << player << " "
        << stats.officialAtsyrauFacedGreater81[player] << "\n";
  }
  out << "# Terminal cause ids: 0=score_82_plus, 1=official_atsyrau, "
         "2=max_steps.\n";
  for (size_t cause = 0; cause < stats.terminalCauseOutcome.size(); ++cause) {
    for (size_t outcome = 0; outcome < OUTCOME_COUNT; ++outcome) {
      out << "%terminalCause" << cause << "_" << outcome_name(outcome) << " "
          << stats.terminalCauseOutcome[cause][outcome] << "\n";
    }
  }
  out << "\n";

  for (size_t margin = 0; margin < stats.winMargin.size(); ++margin) {
    if (stats.winMargin[margin] != 0) {
      out << "%winMargin" << margin << " " << stats.winMargin[margin] << "\n";
    }
  }
  for (size_t move = 0; move < stats.gameLengthMoves.size(); ++move) {
    if (stats.gameLengthMoves[move] != 0) {
      out << "%gameLengthMoves" << move << " " << stats.gameLengthMoves[move]
          << "\n";
    }
  }
  out << "%gameLengthMovesOverflow " << stats.gameLengthMovesOverflow << "\n";
  out << "\n";

  out << "# Exact move counters start from zero for stat files created before "
         "this section existed.\n";
  out << "# Side kumalak sums are stones in the nine pits before that move; "
         "total side excludes kazans.\n";
  out << "%exactMoveTrackedGames " << stats.exactMoveTrackedGames << "\n";
  for (size_t move = 0; move < stats.branchingExactMovePositions.size();
       ++move) {
    if (stats.branchingExactMovePositions[move] == 0) {
      continue;
    }
    out << "%branchingExactMovePositions" << move << " "
        << stats.branchingExactMovePositions[move] << "\n";
    out << "%branchingExactMoveLegalMoveSum" << move << " "
        << stats.branchingExactMoveLegalMoveSum[move] << "\n";
    out << "%branchingExactMove" << move << "AverageLegalMovesX1000 "
        << avg_x1000(stats.branchingExactMoveLegalMoveSum[move],
                     stats.branchingExactMovePositions[move])
        << "\n";
    out << "%whiteSideKumalakSumAtMove" << move << " "
        << stats.whiteSideKumalakSumAtMove[move] << "\n";
    out << "%blackSideKumalakSumAtMove" << move << " "
        << stats.blackSideKumalakSumAtMove[move] << "\n";
    out << "%totalSideKumalakSumAtMove" << move << " "
        << stats.totalSideKumalakSumAtMove[move] << "\n";
    out << "%whiteKazanSumAtMove" << move << " "
        << stats.whiteKazanSumAtMove[move] << "\n";
    out << "%blackKazanSumAtMove" << move << " "
        << stats.blackKazanSumAtMove[move] << "\n";
  }
  out << "%branchingExactMovePositionsOverflow "
      << stats.branchingExactMovePositionsOverflow << "\n";
  out << "%branchingExactMoveLegalMoveSumOverflow "
      << stats.branchingExactMoveLegalMoveSumOverflow << "\n";
  out << "%whiteSideKumalakSumAtMoveOverflow "
      << stats.whiteSideKumalakSumAtMoveOverflow << "\n";
  out << "%blackSideKumalakSumAtMoveOverflow "
      << stats.blackSideKumalakSumAtMoveOverflow << "\n";
  out << "%totalSideKumalakSumAtMoveOverflow "
      << stats.totalSideKumalakSumAtMoveOverflow << "\n";
  out << "%whiteKazanSumAtMoveOverflow "
      << stats.whiteKazanSumAtMoveOverflow << "\n";
  out << "%blackKazanSumAtMoveOverflow "
      << stats.blackKazanSumAtMoveOverflow << "\n";
  out << "\n";

  out << "# Branching bands: 0=moves 1..20, 1=moves 21..60, 2=moves 61+.\n";
  for (size_t band = 0; band < BRANCHING_BAND_COUNT; ++band) {
    out << "%branchingBandPositions" << band << " "
        << stats.branchingPositions[band] << "\n";
    out << "%branchingBandLegalMoveSum" << band << " "
        << stats.branchingLegalMoveSum[band] << "\n";
    out << "%branchingBand" << band << "AverageLegalMovesX1000 "
        << avg_x1000(stats.branchingLegalMoveSum[band],
                     stats.branchingPositions[band])
        << "\n";
  }
}

static void print_benchmark_row(const std::string &name, double seconds,
                                const Stats &stats) {
  double games_per_second = static_cast<double>(stats.games) / seconds;
  double moves_per_second =
      static_cast<double>(stats.moves_of_all_games) / seconds;

  std::cout << std::left << std::setw(18) << name << std::right << std::setw(10)
            << std::fixed << std::setprecision(3) << seconds << std::setw(15)
            << std::fixed << std::setprecision(0) << games_per_second
            << std::setw(15) << std::fixed << std::setprecision(0)
            << moves_per_second << std::setw(14) << stats.moves_of_all_games
            << "\n";
}

static void run_benchmark(Args args) {
  std::cout << "Benchmarking " << args.num_games << " games, seed " << args.seed
            << ", max steps " << args.max_steps << ", threads " << args.threads
            << "\n";
  std::cout << std::left << std::setw(18) << "mode" << std::right
            << std::setw(10) << "seconds" << std::setw(15) << "games/s"
            << std::setw(15) << "moves/s" << std::setw(14) << "moves"
            << "\n";
  std::cout << std::string(72, '-') << "\n";

  const std::array<std::string, 5> modes = {
      "env", "env-fast-rng", "fast", "fast-prebuffer", "fast-parallel",
  };

  for (const std::string &mode : modes) {
    args.mode = mode;
    auto start = std::chrono::steady_clock::now();
    Stats stats = run_mode(args);
    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    print_benchmark_row(mode, seconds, stats);
  }
}

int main(int argc, char *argv[]) {
  try {
    Args args = parse_args(argc, argv);

    if (args.benchmark) {
      run_benchmark(args);
      return 0;
    }

    Stats stats = args.fresh_output ? Stats{} : read_existing_stats(args.stat_path);
    if (args.format_only) {
      write_stats(args, stats);
      std::cout << "Reformatted " << args.stat_path << " (games: "
                << stats.games << ")\n";
      return 0;
    }

    auto start = std::chrono::steady_clock::now();
    Stats run_stats = run_mode(args);
    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    merge_stats(stats, run_stats);

    write_stats(args, stats);
    std::cout << "Played " << run_stats.games << " random games in "
              << std::fixed << std::setprecision(3) << seconds << "s";
    if (seconds > 0.0) {
      std::cout << " (" << std::fixed << std::setprecision(0)
                << static_cast<double>(run_stats.games) / seconds
                << " games/s)";
    }
    std::cout << ".\nStats saved to " << args.stat_path << " (total games: "
              << stats.games << ")\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "generate_billion_games: " << ex.what() << "\n";
    return 1;
  }
}
