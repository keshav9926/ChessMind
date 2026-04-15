#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

// ─── Bitboard type ────────────────────────────────────────────────────────────
using Bitboard = uint64_t;
using Move     = uint32_t;   // encoded move
using Score    = int;

// ─── Pieces ───────────────────────────────────────────────────────────────────
enum Piece  { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE };
enum Color  { WHITE, BLACK, BOTH };
enum Square {
    A1,B1,C1,D1,E1,F1,G1,H1,
    A2,B2,C2,D2,E2,F2,G2,H2,
    A3,B3,C3,D3,E3,F3,G3,H3,
    A4,B4,C4,D4,E4,F4,G4,H4,
    A5,B5,C5,D5,E5,F5,G5,H5,
    A6,B6,C6,D6,E6,F6,G6,H6,
    A7,B7,C7,D7,E7,F7,G7,H7,
    A8,B8,C8,D8,E8,F8,G8,H8,
    NO_SQ = 64
};

// ─── Move encoding (32-bit) ───────────────────────────────────────────────────
// bits  0- 5 : from square
// bits  6-11 : to square
// bits 12-14 : piece type
// bits 15-17 : captured piece (7 = none)
// bits 18-20 : promoted piece (7 = none)
// bit  21    : en-passant flag
// bit  22    : castling flag
// bit  23    : double pawn push flag

inline Move encodeMove(int from, int to, int piece, int cap=6, int promo=6,
                       bool ep=false, bool castle=false, bool dpp=false) {
    return (from) | (to<<6) | (piece<<12) | (cap<<15) | (promo<<18)
         | ((int)ep<<21) | ((int)castle<<22) | ((int)dpp<<23);
}
inline int  moveFrom  (Move m) { return  m        & 0x3F; }
inline int  moveTo    (Move m) { return (m >>  6) & 0x3F; }
inline int  getMvPiece(Move m) { return (m >> 12) & 0x07; }
inline int  moveCap   (Move m) { return (m >> 15) & 0x07; }
inline int  movePromo (Move m) { return (m >> 18) & 0x07; }
inline bool moveEP    (Move m) { return (m >> 21) & 1; }
inline bool moveCastle(Move m) { return (m >> 22) & 1; }
inline bool moveDPP   (Move m) { return (m >> 23) & 1; }
inline bool moveIsCapture(Move m){ return moveCap(m) != 6; }

// ─── Castling rights ──────────────────────────────────────────────────────────
enum CastleRight { WK_CASTLE=1, WQ_CASTLE=2, BK_CASTLE=4, BQ_CASTLE=8 };

// ─── Search constants ─────────────────────────────────────────────────────────
constexpr Score INF       =  1000000;
constexpr Score MATE_SCORE=   900000;
constexpr Score DRAW_SCORE=        0;
constexpr int   MAX_PLY   =       64;
constexpr int   TT_SIZE   = (1 << 22); // 4M entries

// ─── Piece values (centipawns) ────────────────────────────────────────────────
constexpr int PIECE_VALUE[7] = { 100, 320, 330, 500, 900, 20000, 0 };
