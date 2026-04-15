#pragma once
#include "board.h"
#include <vector>

class MoveGen {
public:
    static std::vector<Move> generateLegal(Board& board);
    static std::vector<Move> generateCaptures(Board& board); // for quiescence

private:
    static void generatePawnMoves(Board& b, std::vector<Move>& moves, bool capturesOnly);
    static void generatePieceMoves(Board& b, std::vector<Move>& moves, bool capturesOnly);
    static void generateCastling(Board& b, std::vector<Move>& moves);
    static bool isLegal(Board& b, Move m);
};
