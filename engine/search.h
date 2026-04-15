#pragma once
#include "board.h"
#include "movegen.h"
#include "eval.h"
#include <atomic>
#include <chrono>
#include <string>

// ─── Transposition Table ──────────────────────────────────────────────────────
enum TTFlag { TT_EXACT, TT_LOWER, TT_UPPER };

struct TTEntry {
    uint64_t hash;
    int      depth;
    Score    score;
    Move     bestMove;
    TTFlag   flag;
};

class TranspositionTable {
public:
    std::vector<TTEntry> table;
    TranspositionTable(int size=TT_SIZE) : table(size) {}
    void     clear(){ for(auto& e:table) e.hash=0; }
    TTEntry* probe(uint64_t hash){ auto& e=table[hash%table.size()]; return (e.hash==hash)?&e:nullptr; }
    void     store(uint64_t hash,int depth,Score score,Move best,TTFlag flag){
        auto& e=table[hash%table.size()];
        if(e.hash==0||depth>=e.depth){ e={hash,depth,score,best,flag}; }
    }
};

// ─── Search stats (sent to UI) ────────────────────────────────────────────────
struct SearchInfo {
    long long nodes      = 0;
    long long nodesPerSec= 0;
    int       depth      = 0;
    int       seldepth   = 0;
    Score     score      = 0;
    Move      bestMove   = 0;
    std::string pvLine;
    long long timeMs     = 0;
    long long pruned     = 0;
};

// ─── Search engine ────────────────────────────────────────────────────────────
class Search {
public:
    TranspositionTable tt;
    SearchInfo         info;
    std::atomic<bool>  stop{false};
    int                killerMoves[MAX_PLY][2] = {};
    int                historyTable[2][6][64]  = {};

    // Main search entry — returns best move
    Move startSearch(Board& board, int maxDepth, int timeLimitMs);

    // Accessors for UCI/bridge
    SearchInfo getInfo() const { return info; }

private:
    std::chrono::steady_clock::time_point startTime;
    int                                   timeLimitMs = 2000;

    Score alphaBeta(Board& b, int depth, int ply, Score alpha, Score beta, bool nullOk);
    Score quiescence(Board& b, Score alpha, Score beta, int ply);
    void  orderMoves(Board& b, std::vector<Move>& moves, int ply, Move ttMove);
    int   mvvLva(int victim, int attacker) const;
    int   scoreMove(Board& b, Move m, int ply, Move ttMove) const;
    bool  timeUp() const;
    std::string buildPV(Board& b, int depth);
};
