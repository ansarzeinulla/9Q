import os
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Set plot style for academic papers
plt.rcParams.update({'font.size': 12, 'font.family': 'serif'})

def parse_stats(file_path):
    """Parses raw key-value statistics from the file."""
    stats = {}
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"Stats file not found: {file_path}")
        
    print(f"Parsing statistics from {file_path}...")
    with open(file_path, 'r') as f:
        for line in f:
            if line.startswith('%'):
                parts = line.strip().split()
                if len(parts) == 2:
                    key = parts[0][1:] # Strip '%'
                    try:
                        val = int(parts[1])
                    except ValueError:
                        try:
                            val = float(parts[1])
                        except ValueError:
                            val = parts[1]
                    stats[key] = val
    return stats

def plot_branching_and_decay(stats):
    """Reproduces Figure 1 & Figure 2 using real C++ simulation counters."""
    print("Generating Branching Factor and Board-Material Decay plots...")
    
    total_games = stats.get('featureTrackedGames', 1000000000)
    
    # Calculate games that ended at or before each move step
    # gameLengthMovesX is the number of games that ended at exactly X moves.
    game_ends = {}
    for key, val in stats.items():
        m = re.match(r'^gameLengthMoves(\d+)$', key)
        if m:
            move_idx = int(m.group(1))
            game_ends[move_idx] = val
            
    # Accumulate ended games to get active games at each move index
    max_moves = 120
    ended_cumsum = 0
    games_active = np.zeros(max_moves + 1)
    
    for m in range(1, max_moves + 1):
        games_active[m] = total_games - ended_cumsum
        ended_cumsum += game_ends.get(m, 0)
        
    # Extract branching factor and kazan sums
    half_moves = []
    branching_factors = []
    mean_stones_outside = []
    
    for m in range(1, max_moves + 1):
        bf_key = f"branchingExactMove{m}AverageLegalMovesX1000"
        w_kazan_key = f"whiteKazanSumAtMove{m}"
        b_kazan_key = f"blackKazanSumAtMove{m}"
        
        if bf_key in stats and w_kazan_key in stats and b_kazan_key in stats:
            active = games_active[m]
            if active > 0:
                half_moves.append(m - 1) # 0-indexed for graph
                branching_factors.append(stats[bf_key] / 1000.0)
                
                avg_w_kazan = stats[w_kazan_key] / active
                avg_b_kazan = stats[b_kazan_key] / active
                mean_stones_outside.append(162.0 - (avg_w_kazan + avg_b_kazan))

    fig, ax1 = plt.subplots(figsize=(10, 6))

    color = 'tab:blue'
    ax1.set_xlabel('Half-move index before next move (0 = initial board)')
    ax1.set_ylabel('Average branching factor', color=color)
    ax1.plot(half_moves, branching_factors, color=color, linewidth=2, label="Average legal moves")
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.set_ylim(min(branching_factors) - 0.2, max(branching_factors) + 0.2)

    ax2 = ax1.twinx()  
    color = 'tab:gray'
    ax2.set_ylabel('Mean stones outside kazans', color=color)
    ax2.plot(half_moves, mean_stones_outside, color=color, linestyle='--', label="Mean stones outside kazans")
    ax2.tick_params(axis='y', labelcolor=color)
    ax2.set_ylim(0, 170)

    fig.tight_layout()
    plt.title("Detailed Empirical Branching Factor & Material Decay (First 120 Half-Moves)")
    
    os.makedirs("output", exist_ok=True)
    plt.savefig("output/Figure_1_Branching_Factor.png", dpi=300)
    print("Saved output/Figure_1_Branching_Factor.png")
    plt.close()

def plot_opening_heatmap(stats):
    """Reproduces Figure 3 opening outcomes heatmap using real game outcomes."""
    print("Generating Opening Heatmap...")
    
    # 9x9 grid for White's first pit (rows 1-9) vs Black's first reply (cols 1-9)
    grid = np.full((9, 9), np.nan)
    
    for idx in range(81):
        w_win = stats.get(f"openingPair{idx}_whitewin", 0)
        draw = stats.get(f"openingPair{idx}_draw", 0)
        b_win = stats.get(f"openingPair{idx}_blackwin", 0)
        
        total = w_win + draw + b_win
        if total > 0:
            white_win_rate = (w_win / total) * 100.0
            row = idx // 9 # White's move
            col = idx % 9  # Black's reply
            grid[row, col] = white_win_rate

    plt.figure(figsize=(10, 8))
    # We invert the y-axis to match traditional White first pit order (1 at the top or bottom)
    ax = sns.heatmap(grid, annot=True, fmt=".1f", cmap="vlag", 
                     cbar_kws={'label': 'White win rate (%)'},
                     xticklabels=range(1, 10), yticklabels=range(1, 10))
    
    # Highlight specific key cells discussed in the paper
    # Red highlights for key opening pairs (e.g. Best beginner vs follower)
    ax.add_patch(plt.Rectangle((0, 8), 1, 1, fill=False, edgecolor='red', lw=3)) # row 9 (index 8), col 1 (index 0)
    ax.add_patch(plt.Rectangle((8, 0), 1, 1, fill=False, edgecolor='red', lw=3)) # row 1 (index 0), col 9 (index 8)

    plt.xlabel("Black reply pit")
    plt.ylabel("White first pit")
    plt.title("Opening-pair outcomes under random continuation (1-Billion Games)")
    
    plt.tight_layout()
    plt.savefig("output/Figure_3_Opening_Heatmap.png", dpi=300)
    print("Saved output/Figure_3_Opening_Heatmap.png")
    plt.close()

if __name__ == "__main__":
    print("Starting Academic Reproducibility Script...")
    stats_file = "sample_billion_game_statistics.txt"
    try:
        stats = parse_stats(stats_file)
        plot_branching_and_decay(stats)
        plot_opening_heatmap(stats)
        print("Done! All figures successfully generated in the research/output/ directory.")
    except Exception as e:
        print(f"Error executing reproducibility script: {e}")
