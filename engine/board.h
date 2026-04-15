#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <array>
#include <cassert>

// ─── Precomputed tables (defined in board.cpp) ────────────────────────────────
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];
extern Bitboard RAY[64][8];          // 8 directions
extern uint64_t ZOBRIST_PIECE[2][6][64];
extern uint64_t ZOBRIST_CASTLING[16];
extern uint64_t ZOBRIST_EP[9];        // file 0-7 + no-ep
extern uint64_t ZOBRIST_SIDE;

struct StateInfo {
    int      castlingRights;
    int      epSquare;          // NO_SQ if none
    int      halfMoveClock;
    uint64_t hash;
    Move     lastMove;
    int      capturedPiece;
};

class Board {
public:
    // 12 piece bitboards: [color][piece]
    Bitboard pieces[2][6] = {};
    Bitboard occupied[3]  = {};   // [WHITE],[BLACK],[BOTH]

    int      board[64];           // piece type on square (-1 = empty)
    int      colorOn[64];         // color on square

    int      sideToMove   = WHITE;
    int      castlingRights = WK_CASTLE|WQ_CASTLE|BK_CASTLE|BQ_CASTLE;
    int      epSquare     = NO_SQ;
    int      halfMoveClock= 0;
    int      fullMove     = 1;
    uint64_t hash         = 0;

    std::vector<StateInfo> history;

    // ── Init ──────────────────────────────────────────────────────────────────
    static void initTables();
    void        setFen(const std::string& fen);
    std::string toFen() const;
    void        reset() { setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }

    // ── Square helpers ────────────────────────────────────────────────────────
    Bitboard getPieces(int color, int piece) const { return pieces[color][piece]; }
    Bitboard getOccupied()                   const { return occupied[BOTH]; }
    bool     isOccupied(int sq)              const { return (occupied[BOTH] >> sq) & 1; }
    int      pieceOn(int sq)                 const { return board[sq]; }

    // ── Attack generation ─────────────────────────────────────────────────────
    Bitboard attacksFrom(int sq, int piece, int color) const;
    Bitboard attackersTo(int sq, int color) const;
    bool     isInCheck(int color) const;
    bool     isSquareAttacked(int sq, int attacker) const;

    // ── Move make/undo ────────────────────────────────────────────────────────
    void makeMove(Move m);
    void undoMove(Move m);
    void makeNullMove();
    void undoNullMove();

    // ── Utility ───────────────────────────────────────────────────────────────
    bool     isRepetition() const;
    bool     isInsufficientMaterial() const;
    int      kingSquare(int color) const;
    uint64_t computeHash() const;

private:
    void putPiece(int color, int piece, int sq);
    void removePiece(int color, int piece, int sq);
    void movePiece(int color, int piece, int from, int to);
};

// ─── Global board init (call once at startup) ─────────────────────────────────
void initAll();
