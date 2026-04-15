#include "board.h"
#include "movegen.h"
#include <iostream>
#include <cassert>
#include <chrono>

// ─── Perft: counts leaf nodes at given depth ───────────────────────────────
long long perft(Board& board, int depth){
    if(depth==0) return 1;
    auto moves=MoveGen::generateLegal(board);
    if(depth==1) return moves.size();
    long long nodes=0;
    for(Move m:moves){
        board.makeMove(m);
        nodes+=perft(board,depth-1);
        board.undoMove(m);
    }
    return nodes;
}

struct PerftCase {
    const char* name;
    const char* fen;
    int depth;
    long long expected;
};

// Known correct perft values from chessprogramming.org
PerftCase CASES[] = {
    // Starting position
    {"Start pos d1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20},
    {"Start pos d2", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400},
    {"Start pos d3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902},
    {"Start pos d4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
    // Position 2 (kiwipete) - tests castling, ep, promotions
    {"Kiwipete d1",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
    {"Kiwipete d2",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
    {"Kiwipete d3",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
    // Position 3 - tests promotions
    {"Promo d4",     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
    // Position 5 - promotions + castling
    {"Pos5 d1",   "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",  1,   44},
    {"Pos5 d2",   "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",  2, 1486},
    {"Pos5 d3",   "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",  3, 62379},
};

int main(){
    initAll();
    Board board;

    int passed=0, failed=0;
    auto total_start=std::chrono::steady_clock::now();

    std::cout<<"ChessMind Perft Test Suite\n";
    std::cout<<std::string(60, '-')<<"\n";

    for(auto& tc : CASES){
        board.setFen(tc.fen);
        auto t0=std::chrono::steady_clock::now();
        long long result=perft(board,tc.depth);
        long long ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-t0).count();

        bool ok=(result==tc.expected);
        if(ok) passed++; else failed++;

        std::cout<<(ok?"  ✓  ":"  ✗  ")<<tc.name
                 <<" depth "<<tc.depth
                 <<": got "<<result
                 <<" expected "<<tc.expected;
        if(!ok) std::cout<<" ← FAIL diff="<<(result-tc.expected);
        if(ms>0) std::cout<<"  ("<<ms<<"ms, "<<(result*1000/(ms+1)/1000)<<"kn/s)";
        std::cout<<"\n";
    }

    long long total_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-total_start).count();

    std::cout<<std::string(60, '-')<<"\n";
    std::cout<<"Results: "<<passed<<"/"<<(passed+failed)<<" passed in "<<total_ms<<"ms\n";

    if(failed>0){
        std::cout<<"MOVE GENERATION HAS BUGS — fix before using engine\n";
        return 1;
    }
    std::cout<<"All tests passed — move generation is correct!\n";
    return 0;
}
