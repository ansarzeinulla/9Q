#include "../../engine/dag_search.hpp"
#include "../../engine/evaluation.hpp"
#include "../../engine/minimax_engine.hpp"
#include "../../engine/togyzkumalak_rules.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace {

ToguzEnv env;
HeuristicEvaluator evaluator;
std::string state_cache;
std::string fen_cache;
std::string error_cache;
std::string bot_cache = "{\"move\":-1,\"bot\":\"none\",\"elapsedMs\":0,\"completedDepth\":0,\"eval\":0,\"status\":\"idle\"}";
std::string move_cache = "{\"move\":-1,\"landingPit\":-1,\"notation\":\"\",\"suffix\":\"\",\"captured\":false,\"tuzdyk\":false,\"side\":\"\"}";

std::string lower_copy(const char* value) {
    std::string result = value == nullptr ? "" : value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string normalize_bot(const char* value) {
    std::string bot = lower_copy(value);
    if (bot == "random" || bot == "randombot") return "randombot";
    if (bot == "ai" || bot == "ai4" || bot == "minimax") return "minimax";
    if (bot == "dag" || bot == "dag4" || bot == "filter" || bot == "filter4") return "dag4";
    if (bot == "human") return "human";
    return "randombot";
}

std::string trim_copy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::string item;
    std::istringstream in(value);
    while (std::getline(in, item, delimiter)) {
        result.push_back(trim_copy(item));
    }
    return result;
}

bool parse_nonnegative_int(const std::string& text, int& out) {
    std::string value = trim_copy(text);
    if (value.empty()) return false;
    long long parsed = 0;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
        parsed = parsed * 10 + (ch - '0');
        if (parsed > 10000) return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

bool parse_int_list(const std::string& text, size_t expected, int min_value,
                    int max_value, std::vector<int>& out) {
    std::vector<std::string> parts = split(text, ',');
    if (parts.size() != expected) return false;
    out.clear();
    for (const std::string& part : parts) {
        int parsed = 0;
        if (!parse_nonnegative_int(part, parsed)) return false;
        if (parsed < min_value || parsed > max_value) return false;
        out.push_back(parsed);
    }
    return true;
}

bool parse_tuzdyk_list(const std::string& text, std::array<int, 2>& out) {
    std::vector<std::string> parts = split(text, ',');
    if (parts.size() != 2) return false;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == "-") {
            out[i] = -1;
            continue;
        }
        int parsed = 0;
        if (!parse_nonnegative_int(parts[i], parsed)) return false;
        if (parsed < 1 || parsed > NUM_PITS) return false;
        out[i] = parsed - 1;
    }
    if (out[0] != -1 && out[0] == out[1]) return false;
    return true;
}

std::string pit_csv(int offset) {
    std::ostringstream out;
    for (int i = 0; i < NUM_PITS; ++i) {
        if (i > 0) out << ",";
        out << env.board.get(offset + i);
    }
    return out.str();
}

std::string tuzdyk_text(int value) {
    if (value < 0) return "-";
    return std::to_string(value + 1);
}

std::string make_fen_string() {
    std::ostringstream out;
    out << pit_csv(0) << "/" << pit_csv(NUM_PITS) << " ";
    out << env.kazans[PLAYER_1] << "," << env.kazans[PLAYER_2] << " ";
    out << tuzdyk_text(env.tuzduks[PLAYER_1]) << "," << tuzdyk_text(env.tuzduks[PLAYER_2]) << " ";
    out << (env.to_play == PLAYER_1 ? "w" : "b") << " ";
    out << env.steps;
    return out.str();
}

std::string json_array(const std::vector<int>& values) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << ",";
        out << values[i];
    }
    out << "]";
    return out.str();
}

std::string board_side_json(int offset) {
    std::ostringstream out;
    out << "[";
    for (int i = 0; i < NUM_PITS; ++i) {
        if (i > 0) out << ",";
        out << env.board.get(offset + i);
    }
    out << "]";
    return out.str();
}

std::string winner_name() {
    if (env.winner_code == PLAYER_1) return "white";
    if (env.winner_code == PLAYER_2) return "black";
    if (env.winner_code == -1) return "draw";
    return "";
}

std::string result_label() {
    if (env.winner_code == PLAYER_1) return "White wins";
    if (env.winner_code == PLAYER_2) return "Black wins";
    if (env.winner_code == -1) return "Draw";
    return "";
}

std::string make_state_json() {
    std::ostringstream out;
    std::vector<int> legal = env.generate_moves();
    std::string winner = winner_name();
    std::string result = result_label();

    out << "{";
    out << "\"pits\":{\"white\":" << board_side_json(0)
        << ",\"black\":" << board_side_json(NUM_PITS) << "},";
    out << "\"kazans\":{\"white\":" << env.kazans[PLAYER_1]
        << ",\"black\":" << env.kazans[PLAYER_2] << "},";
    out << "\"tuzdyks\":{\"white\":" << env.tuzduks[PLAYER_1]
        << ",\"black\":" << env.tuzduks[PLAYER_2] << "},";
    out << "\"toPlay\":\"" << (env.to_play == PLAYER_1 ? "white" : "black") << "\",";
    out << "\"toPlayIndex\":" << env.to_play << ",";
    out << "\"steps\":" << env.steps << ",";
    out << "\"gameOver\":" << (env.is_game_over() ? "true" : "false") << ",";
    out << "\"winnerCode\":" << env.winner_code << ",";
    out << "\"winner\":\"" << winner << "\",";
    out << "\"result\":\"" << result << "\",";
    out << "\"legal\":" << json_array(legal) << ",";
    out << "\"repetitionCount\":" << env.current_repetition_count();
    out << "}";
    return out.str();
}

struct BotResult {
    int move = -1;
    int completed_depth = 0;
    double eval = 0.0;
    std::string status = "ok";
};

struct AppliedMove {
    int move = -1;
    int stones_before = 0;
    int landing_pit = -1;
    int mover = PLAYER_1;
    bool captured = false;
    bool tuzdyk = false;
    std::string notation;
};

std::string move_side_name(int player) {
    return player == PLAYER_1 ? "white" : "black";
}

AppliedMove apply_move_with_record(int move) {
    AppliedMove record;
    record.move = move;
    record.mover = env.to_play;
    record.stones_before = env.board.get(record.mover * NUM_PITS + move);
    int start = record.mover * NUM_PITS + move;
    int landing = landing_pit(start, record.stones_before);
    record.landing_pit = landing % NUM_PITS;

    int before_kazan = env.kazans[record.mover];
    int before_tuzdyk = env.tuzduks[record.mover];

    env.step(move);

    record.tuzdyk = env.tuzduks[record.mover] != before_tuzdyk;
    record.captured = !record.tuzdyk && env.kazans[record.mover] > before_kazan;
    record.notation = std::to_string(move + 1) + std::to_string(record.landing_pit + 1);
    if (record.tuzdyk) {
        record.notation += "x";
    } else if (record.captured) {
        record.notation += "+";
    }
    return record;
}

std::string make_move_json(const AppliedMove& record, const std::string& bot,
                           int completed_depth, double eval, double elapsed_ms,
                           const std::string& status) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"move\":" << record.move << ",";
    out << "\"stonesBefore\":" << record.stones_before << ",";
    out << "\"landingPit\":" << record.landing_pit << ",";
    out << "\"notation\":\"" << record.notation << "\",";
    out << "\"suffix\":\"" << (record.tuzdyk ? "x" : (record.captured ? "+" : "")) << "\",";
    out << "\"captured\":" << (record.captured ? "true" : "false") << ",";
    out << "\"tuzdyk\":" << (record.tuzdyk ? "true" : "false") << ",";
    out << "\"side\":\"" << move_side_name(record.mover) << "\",";
    out << "\"bot\":\"" << bot << "\",";
    out << "\"elapsedMs\":" << elapsed_ms << ",";
    out << "\"completedDepth\":" << completed_depth << ",";
    out << "\"eval\":" << eval << ",";
    out << "\"status\":\"" << status << "\"";
    out << "}";
    return out.str();
}

bool apply_fen(const std::string& fen) {
    std::string input = trim_copy(fen);
    if (input.empty()) {
        env.reset();
        return true;
    }

    std::istringstream fields(input);
    std::vector<std::string> parts;
    std::string part;
    while (fields >> part) {
        parts.push_back(part);
    }
    if (parts.size() != 5) return false;

    std::vector<std::string> side_parts = split(parts[0], '/');
    if (side_parts.size() != 2) return false;

    std::vector<int> white_pits;
    std::vector<int> black_pits;
    std::vector<int> kazans;
    std::array<int, 2> tuzduks = {-1, -1};
    int steps = 0;

    if (!parse_int_list(side_parts[0], NUM_PITS, 0, TOTAL_STONES, white_pits)) return false;
    if (!parse_int_list(side_parts[1], NUM_PITS, 0, TOTAL_STONES, black_pits)) return false;
    if (!parse_int_list(parts[1], 2, 0, TOTAL_STONES, kazans)) return false;
    if (!parse_tuzdyk_list(parts[2], tuzduks)) return false;
    if (!parse_nonnegative_int(parts[4], steps)) return false;
    if (steps > 10000) return false;

    std::string side = lower_copy(parts[3].c_str());
    int to_play = -1;
    if (side == "w" || side == "white") {
        to_play = PLAYER_1;
    } else if (side == "b" || side == "black") {
        to_play = PLAYER_2;
    } else {
        return false;
    }

    int total = kazans[0] + kazans[1];
    for (int value : white_pits) total += value;
    for (int value : black_pits) total += value;
    if (total != TOTAL_STONES) return false;

    env.board.side1 = 0;
    env.board.side2 = 0;
    for (int i = 0; i < NUM_PITS; ++i) {
        env.board.set(i, white_pits[i]);
        env.board.set(NUM_PITS + i, black_pits[i]);
    }
    env.kazans = {kazans[0], kazans[1]};
    env.tuzduks = tuzduks;
    env.to_play = to_play;
    env.steps = steps;
    env.winner_code = -2;

    if (env.kazans[PLAYER_1] >= WIN_THRESHOLD || env.kazans[PLAYER_2] >= WIN_THRESHOLD ||
        env.steps >= env.max_steps) {
        env.determine_winner_by_kazans();
    }

    env.reset_repetition_history();
    return true;
}

BotResult choose_bot_move(const std::string& bot, double seconds_per_move) {
    BotResult result;
    if (env.is_game_over()) {
        result.status = "game-over";
        return result;
    }

    if (bot == "randombot") {
        result.move = env.get_random_move();
        result.status = "timeless";
        return result;
    }

    seconds_per_move = std::clamp(seconds_per_move, 0.001, 30.0);

    if (bot == "minimax") {
        auto search = minimax_engine::get_best_move_timed(env, seconds_per_move, 64, evaluator);
        result.move = search.move;
        result.completed_depth = search.completed_depth;
        result.eval = search.eval;
        result.status = search.timed_out ? "timed" : "ok";
        return result;
    }

    if (bot == "dag4") {
        auto search = dag_search::get_best_move_timed(env, seconds_per_move, 64, evaluator);
        result.move = search.move;
        result.completed_depth = search.completed_depth;
        result.eval = search.eval;
        result.status = search.timed_out ? "timed" : "ok";
        return result;
    }

    result.status = "unsupported";
    return result;
}

std::string make_bot_json(const std::string& bot, const BotResult& result, double elapsed_ms) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"move\":" << result.move << ",";
    out << "\"bot\":\"" << bot << "\",";
    out << "\"elapsedMs\":" << elapsed_ms << ",";
    out << "\"completedDepth\":" << result.completed_depth << ",";
    out << "\"eval\":" << result.eval << ",";
    out << "\"status\":\"" << result.status << "\"";
    out << "}";
    return out.str();
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* tg_version() {
    return "9Q Wasm engine 0.1";
}

EMSCRIPTEN_KEEPALIVE
void tg_reset() {
    env.reset();
    bot_cache = "{\"move\":-1,\"bot\":\"none\",\"elapsedMs\":0,\"completedDepth\":0,\"eval\":0,\"status\":\"idle\"}";
    move_cache = "{\"move\":-1,\"landingPit\":-1,\"notation\":\"\",\"suffix\":\"\",\"captured\":false,\"tuzdyk\":false,\"side\":\"\"}";
    error_cache = "";
}

EMSCRIPTEN_KEEPALIVE
const char* tg_state_json() {
    state_cache = make_state_json();
    return state_cache.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* tg_fen_string() {
    fen_cache = make_fen_string();
    return fen_cache.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* tg_last_error() {
    return error_cache.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* tg_last_bot_json() {
    return bot_cache.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* tg_last_move_json() {
    return move_cache.c_str();
}

EMSCRIPTEN_KEEPALIVE
int tg_set_fen(const char* fen) {
    bool ok = apply_fen(fen == nullptr ? "" : fen);
    if (!ok) {
        error_cache = "fen_invalid";
        return 0;
    }
    bot_cache = "{\"move\":-1,\"bot\":\"none\",\"elapsedMs\":0,\"completedDepth\":0,\"eval\":0,\"status\":\"idle\"}";
    move_cache = "{\"move\":-1,\"landingPit\":-1,\"notation\":\"\",\"suffix\":\"\",\"captured\":false,\"tuzdyk\":false,\"side\":\"\"}";
    error_cache = "";
    return 1;
}

EMSCRIPTEN_KEEPALIVE
int tg_make_move(int pit) {
    if (env.is_game_over()) return -1;
    std::vector<int> legal = env.generate_moves();
    if (std::find(legal.begin(), legal.end(), pit) == legal.end()) return 0;
    AppliedMove record = apply_move_with_record(pit);
    move_cache = make_move_json(record, "human", 0, 0.0, 0.0, "ok");
    return 1;
}

EMSCRIPTEN_KEEPALIVE
int tg_bot_move(const char* bot_name, double seconds_per_move) {
    std::string bot = normalize_bot(bot_name);
    auto start = std::chrono::steady_clock::now();
    BotResult result = choose_bot_move(bot, seconds_per_move);
    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (result.move >= 0) {
        AppliedMove record = apply_move_with_record(result.move);
        move_cache = make_move_json(record, bot, result.completed_depth,
                                    result.eval, elapsed_ms, result.status);
    } else {
        move_cache = "{\"move\":-1,\"landingPit\":-1,\"notation\":\"\",\"suffix\":\"\",\"captured\":false,\"tuzdyk\":false,\"side\":\"\"}";
    }
    bot_cache = make_bot_json(bot, result, elapsed_ms);
    return result.move;
}

}
