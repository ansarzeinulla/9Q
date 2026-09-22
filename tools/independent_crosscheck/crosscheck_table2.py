"""Independent cross-check of the exhaustive early-game search (paper, Table 2).

Recomputes Table 2 with a Togyzkumalak implementation written by a different
author, yernarsha/togyz_py, used unmodified. For each ply it counts the unique
non-terminal positions before the ply (frontier), all legal moves from them
(continuations), terminal children, and the Tuzdyk state of every child, then
compares the row with the paper.

togyz_py has no license, so it is not vendored here. Clone it next to 9Q:

    git clone https://github.com/yernarsha/togyz_py.git
    git -C togyz_py checkout f2c6f9b042031ef7d4df972e29d690d72d485ad1
    python3 tools/independent_crosscheck/crosscheck_table2.py --togyz-py togyz_py

Plies 1-8 take ~2 minutes and ~1 GB RAM on an Apple M2 Pro (ply 8 alone ~100 s).
"""

import argparse
import sys
import time

# Table 2 of the paper:
# ply: (frontier, continuations, terminals, no tuzdyk, Beginner only, Follower only, both)
PAPER_TABLE_2 = {
    1: (1, 9, 0, 9, 0, 0, 0),
    2: (9, 73, 0, 73, 0, 0, 0),
    3: (73, 613, 0, 613, 0, 0, 0),
    4: (613, 5199, 0, 4934, 0, 265, 0),
    5: (5199, 43184, 0, 38675, 2389, 2080, 40),
    6: (42841, 357277, 0, 304840, 18087, 33259, 1091),
    7: (350462, 2924670, 0, 2371318, 288414, 245150, 19788),
    8: (2846239, 23876487, 0, 18502155, 2116316, 3012001, 246015),
    9: (23068632, 193199319, 0, 144048551, 24509770, 21789072, 2851926),
    10: (185464053, 1559006339, 0, 1121297406, 176681476, 232233978, 28793479),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--togyz-py", required=True, help="path to a togyz_py checkout")
    parser.add_argument("--max-ply", type=int, default=8,
                        help="last ply to check (default 8; 9-10 need tens of GB of RAM)")
    args = parser.parse_args()

    sys.path.insert(0, args.togyz_py)
    from togboard import TogBoard  # noqa: E402  (the independent implementation)

    def children(fields):
        color = fields[22]
        for pit in range(1, 10):
            if fields[pit + color * 9 - 1] > 0:
                board = TogBoard.__new__(TogBoard)
                board.fields = list(fields)
                board.finished = False
                board.gameResult = -2
                board.moves = []
                board.makeMove(pit)
                yield board.fields, board.finished

    # fields[18] = Beginner's tuzdyk, fields[19] = Follower's tuzdyk (0 = none)
    def tuzdyk_class(fields):
        return {(False, False): 0, (True, False): 1,
                (False, True): 2, (True, True): 3}[(fields[18] != 0, fields[19] != 0)]

    def key(fields):  # values are -1..162; shift by 1 to fit in a byte
        return bytes(x + 1 for x in fields)

    frontier = {key(TogBoard().fields)}
    all_match = True
    for ply in range(1, args.max_ply + 1):
        start = time.time()
        continuations = terminals = 0
        classes = [0, 0, 0, 0]
        next_frontier = set()
        for k in frontier:
            fields = [x - 1 for x in k]
            for child, finished in children(fields):
                continuations += 1
                classes[tuzdyk_class(child)] += 1
                if finished:
                    terminals += 1
                else:
                    next_frontier.add(key(child))
        row = (len(frontier), continuations, terminals, *classes)
        expected = PAPER_TABLE_2.get(ply)
        verdict = "MATCH" if row == expected else "DIFF"
        all_match &= row == expected
        print(f"ply {ply:2d}: frontier {row[0]:>11,}  continuations {row[1]:>13,}  "
              f"terminals {row[2]}  tuzdyk classes {row[3:]}  -> {verdict} "
              f"({time.time() - start:.0f}s)", flush=True)
        if row != expected:
            print(f"         paper: {expected}")
        frontier = next_frontier

    print("\nAll rows match the paper." if all_match else "\nSome rows differ from the paper.")
    return 0 if all_match else 1


if __name__ == "__main__":
    sys.exit(main())
