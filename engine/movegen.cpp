#include "movegen.h"

static inline int lsb2(Bitboard b){ return __builtin_ctzll(b); }

bool MoveGen::isLegal(Board& b, Move m){
    b.makeMove(m);
    bool legal = !b.isInCheck(b.sideToMove^1); // side that just moved
    b.undoMove(m);
    return legal;
}

void MoveGen::generatePawnMoves(Board& b, std::vector<Move>& moves, bool capturesOnly){
    int us=b.sideToMove, them=us^1;
    Bitboard pawns=b.pieces[us][PAWN];
    Bitboard occ=b.occupied[BOTH], theirPieces=b.occupied[them];
    int push=(us==WHITE)?8:-8;
    int startRank=(us==WHITE)?1:6, promoRank=(us==WHITE)?6:1;

    while(pawns){
        int from=lsb2(pawns); pawns&=pawns-1;
        int r=from>>3;

        // Captures
        Bitboard atk=PAWN_ATTACKS[us][from] & theirPieces;
        while(atk){
            int to=lsb2(atk); atk&=atk-1;
            int cap=b.board[to];
            if(r==promoRank){
                for(int pr=KNIGHT;pr<=QUEEN;pr++)
                    moves.push_back(encodeMove(from,to,PAWN,cap,pr));
            } else {
                moves.push_back(encodeMove(from,to,PAWN,cap));
            }
        }

        // En passant
        if(b.epSquare!=NO_SQ && (PAWN_ATTACKS[us][from]>>b.epSquare&1))
            moves.push_back(encodeMove(from,b.epSquare,PAWN,PAWN,6,true));

        if(!capturesOnly){
            // Single push
            int to1=from+push;
            if(to1>=0&&to1<64&&!b.isOccupied(to1)){
                if(r==promoRank){
                    for(int pr=KNIGHT;pr<=QUEEN;pr++)
                        moves.push_back(encodeMove(from,to1,PAWN,6,pr));
                } else {
                    moves.push_back(encodeMove(from,to1,PAWN));
                    // Double push
                    if(r==startRank){
                        int to2=from+push*2;
                        if(!b.isOccupied(to2))
                            moves.push_back(encodeMove(from,to2,PAWN,6,6,false,false,true));
                    }
                }
            }
        }
    }
}

void MoveGen::generatePieceMoves(Board& b, std::vector<Move>& moves, bool capturesOnly){
    int us=b.sideToMove, them=us^1;
    Bitboard targets = capturesOnly ? b.occupied[them] : ~b.occupied[us];

    for(int piece=KNIGHT;piece<=KING;piece++){
        Bitboard bb=b.pieces[us][piece];
        while(bb){
            int from=lsb2(bb); bb&=bb-1;
            Bitboard atk=b.attacksFrom(from,piece,us) & targets;
            while(atk){
                int to=lsb2(atk); atk&=atk-1;
                int cap=b.board[to]; if(cap==-1) cap=6;
                moves.push_back(encodeMove(from,to,piece,cap));
            }
        }
    }
}

void MoveGen::generateCastling(Board& b, std::vector<Move>& moves){
    int us=b.sideToMove;
    if(b.isInCheck(us)) return;
    Bitboard occ=b.occupied[BOTH];

    if(us==WHITE){
        // Kingside: squares F1,G1 must be empty and not attacked
        if((b.castlingRights&WK_CASTLE) &&
           !(occ & ((1ULL<<F1)|(1ULL<<G1))) &&
           !b.isSquareAttacked(F1,BLACK) &&
           !b.isSquareAttacked(G1,BLACK))
            moves.push_back(encodeMove(E1,G1,KING,6,6,false,true));
        // Queenside: squares B1,C1,D1 must be empty; D1,C1 must not be attacked
        if((b.castlingRights&WQ_CASTLE) &&
           !(occ & ((1ULL<<B1)|(1ULL<<C1)|(1ULL<<D1))) &&
           !b.isSquareAttacked(D1,BLACK) &&
           !b.isSquareAttacked(C1,BLACK))
            moves.push_back(encodeMove(E1,C1,KING,6,6,false,true));
    } else {
        // Kingside
        if((b.castlingRights&BK_CASTLE) &&
           !(occ & ((1ULL<<F8)|(1ULL<<G8))) &&
           !b.isSquareAttacked(F8,WHITE) &&
           !b.isSquareAttacked(G8,WHITE))
            moves.push_back(encodeMove(E8,G8,KING,6,6,false,true));
        // Queenside
        if((b.castlingRights&BQ_CASTLE) &&
           !(occ & ((1ULL<<B8)|(1ULL<<C8)|(1ULL<<D8))) &&
           !b.isSquareAttacked(D8,WHITE) &&
           !b.isSquareAttacked(C8,WHITE))
            moves.push_back(encodeMove(E8,C8,KING,6,6,false,true));
    }
}

std::vector<Move> MoveGen::generateLegal(Board& board){
    std::vector<Move> pseudo, legal;
    pseudo.reserve(64);
    generatePawnMoves(board,pseudo,false);
    generatePieceMoves(board,pseudo,false);
    generateCastling(board,pseudo);
    legal.reserve(pseudo.size());
    for(Move m:pseudo) if(isLegal(board,m)) legal.push_back(m);
    return legal;
}

std::vector<Move> MoveGen::generateCaptures(Board& board){
    std::vector<Move> pseudo, legal;
    pseudo.reserve(32);
    generatePawnMoves(board,pseudo,true);
    generatePieceMoves(board,pseudo,true);
    legal.reserve(pseudo.size());
    for(Move m:pseudo) if(isLegal(board,m)) legal.push_back(m);
    return legal;
}
