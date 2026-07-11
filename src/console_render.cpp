#include "togyz/console_render.hpp"

#include <iomanip>

namespace togyz_render {

void render_console(std::ostream& os, const Bitboard& board, const std::array<int, 2>& kazans,
                    const std::array<int, 2>& tuzduks, const std::string& p1_name,
                    const std::string& p2_name) {
  os << "Kazan: " << p1_name << "=" << kazans[0] << " " << p2_name << "=" << kazans[1] << "\n";

  // P2 pits: 17...9 (side2 indices 8...0)
  for (int i = 8; i >= 0; --i) {
    if (tuzduks[0] == i)
      os << "[ X]";
    else
      os << "[" << std::setw(2) << board.get(i + 9) << "]";
  }
  os << "\n";

  // P1 pits: 0...8 (side1 indices 0...8)
  for (int i = 0; i < 9; ++i) {
    if (tuzduks[1] == i)
      os << "[ X]";
    else
      os << "[" << std::setw(2) << board.get(i) << "]";
  }
  os << "\n";
}

void render_console(std::ostream& os, const ToguzEnv& env, const std::string& p1_name,
                    const std::string& p2_name) {
  render_console(os, env.board, env.kazans, env.tuzduks, p1_name, p2_name);
}

}  // namespace togyz_render
