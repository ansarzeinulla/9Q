#ifndef CONSOLE_RENDER_HPP
#define CONSOLE_RENDER_HPP

#include <array>
#include <ostream>
#include <string>

#include "togyz/togyzkumalak_rules.hpp"

namespace togyz_render {

void render_console(std::ostream& os, const Bitboard& board, const std::array<int, 2>& kazans,
                    const std::array<int, 2>& tuzduks, const std::string& p1_name = "P1",
                    const std::string& p2_name = "P2");

void render_console(std::ostream& os, const ToguzEnv& env, const std::string& p1_name = "P1",
                    const std::string& p2_name = "P2");

}  // namespace togyz_render

#endif
