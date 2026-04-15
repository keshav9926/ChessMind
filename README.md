# ChessMind ♟️
**AI chess engine — Minimax + Alpha-Beta pruning · ~2000 ELO · Stockfish hints**

---

## Run in 2 commands

### Linux / macOS
```bash
chmod +x setup.sh run.sh
./setup.sh        # installs everything, compiles engine (~30 sec)
./run.sh          # starts server + opens browser
```

### Windows
```
Double-click setup.bat   # installs + compiles
Double-click run.bat     # starts game
```

---

## What setup.sh does
1. Checks for g++, Python3, Stockfish
2. Installs Python packages (fastapi, uvicorn, python-chess)
3. Compiles the C++ chess engine with O3 optimization
4. Runs 11 perft tests to verify move generation is correct
5. Copies engine binary to bridge/

## What run.sh does
1. Starts the Python WebSocket bridge on port 8000
2. Opens frontend/index.html in your browser
3. Keeps the bridge running (auto-restarts on crash)

---

## Requirements
| Requirement | Install |
|---|---|
| g++ (C++17) | `sudo apt install g++` / `xcode-select --install` |
| Python 3.10+ | python.org |
| Stockfish | `sudo apt install stockfish` / `brew install stockfish` |

Stockfish is optional — the game works without it but hints will be disabled.

---

## How to play
1. Select difficulty and your color in the left panel
2. Click **Start Game**
3. Click any piece to see legal moves highlighted
4. Click **Get hints** for Stockfish's top 3 suggestions
5. Watch the engine stats panel (depth, nodes/sec, best line) as AI thinks

## Difficulty levels
| Level | Depth | Approx ELO |
|---|---|---|
| Beginner | 2 | ~800 |
| Intermediate | 5 | ~1400 |
| Hard | 8 | ~1800 |
| Expert | 12 | ~2100 |

---

## Project structure
```
chessmind/
├── setup.sh          ← Run this first (Linux/macOS)
├── run.sh            ← Run this to play (Linux/macOS)
├── setup.bat         ← Run this first (Windows)
├── run.bat           ← Run this to play (Windows)
├── engine/
│   ├── types.h       ← Bitboard types, move encoding
│   ├── board.h/cpp   ← Board representation, FEN, Zobrist hashing
│   ├── movegen.h/cpp ← Legal move generation (11/11 perft tests pass)
│   ├── eval.h/cpp    ← Evaluation: material, PST, king safety, pawns
│   ├── search.h/cpp  ← Alpha-Beta, iterative deepening, TT, LMR
│   ├── main.cpp      ← UCI protocol entry point
│   ├── book.h        ← Opening book (20+ lines)
│   └── perft.cpp     ← Move generation correctness tests
├── bridge/
│   └── server.py     ← FastAPI WebSocket bridge + Stockfish hints
└── frontend/
    └── index.html    ← Full React chess UI (self-contained)
```

---

## Troubleshooting

**Red "Disconnected" dot in browser**
→ Bridge isn't running. Run `./run.sh` and keep that terminal open.

**Stockfish hints not working**
→ Run `which stockfish` — if empty, install it. Or set path: `export STOCKFISH_PATH=/path/to/stockfish`

**Compilation error on macOS**
→ Run `xcode-select --install` then retry setup.sh

**Port 8000 already in use**
→ `lsof -ti :8000 | xargs kill -9` then run.sh again

**Windows: engine not found**
→ Make sure MinGW is installed and `g++` is in PATH. Try WSL2 instead.
