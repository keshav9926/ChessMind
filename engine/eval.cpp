#include "eval.h"

static inline int lsb3(Bitboard b){ return __builtin_ctzll(b); }
static inline int pop(Bitboard b){ return __builtin_popcountll(b); }

bool Evaluator::isEndgame(const Board& b){
    // Endgame: no queens, or queen + at most one minor piece each side
    int queens = pop(b.pieces[WHITE][QUEEN]) + pop(b.pieces[BLACK][QUEEN]);
    if(queens==0) return true;
    int wMinor = pop(b.pieces[WHITE][KNIGHT])+pop(b.pieces[WHITE][BISHOP]);
    int bMinor = pop(b.pieces[BLACK][KNIGHT])+pop(b.pieces[BLACK][BISHOP]);
    return (queens==1 && wMinor+bMinor<=2);
}

Score Evaluator::evalPawnStructure(Board& b, int color){
    Score score=0;
    Bitboard pawns=b.pieces[color][PAWN];
    Bitboard opPawns=b.pieces[color^1][PAWN];
    Bitboard tmp=pawns;

    while(tmp){
        int sq=lsb3(tmp); tmp&=tmp-1;
        int file=sq&7, rank=sq>>3;

        // Doubled pawns penalty
        Bitboard fileMask=0x0101010101010101ULL<<file;
        if(pop(pawns&fileMask)>1) score-=15;

        // Isolated pawn penalty
        Bitboard adjFiles=0;
        if(file>0) adjFiles|=0x0101010101010101ULL<<(file-1);
        if(file<7) adjFiles|=0x0101010101010101ULL<<(file+1);
        if(!(pawns&adjFiles)) score-=20;

        // Passed pawn bonus
        Bitboard passedMask=fileMask|adjFiles;
        if(color==WHITE) passedMask&=~((1ULL<<((rank+1)*8))-1); // squares ahead
        else             passedMask&=(1ULL<<(rank*8))-1;
        if(!(opPawns&passedMask)){
            int advancement = (color==WHITE)?rank:(7-rank);
            score += 20 + advancement*advancement*5; // bigger bonus closer to promotion
        }
    }
    return score;
}

Score Evaluator::evalKingSafety(Board& b, int color){
    Score score=0;
    int kingSq=b.kingSquare(color);
    int kingFile=kingSq&7, kingRank=kingSq>>3;

    // Pawn shield bonus
    Bitboard shield=0;
    if(color==WHITE && kingRank<7){
        for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++)
            shield|=1ULL<<((kingRank+1)*8+f);
    } else if(color==BLACK && kingRank>0){
        for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++)
            shield|=1ULL<<((kingRank-1)*8+f);
    }
    score += pop(b.pieces[color][PAWN]&shield) * 15;

    // Open file near king penalty
    for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++){
        Bitboard fileBB=0x0101010101010101ULL<<f;
        if(!(b.pieces[color][PAWN]&fileBB)) score-=20;
        if(!(b.pieces[color][PAWN]&fileBB) && !(b.pieces[color^1][PAWN]&fileBB)) score-=10;
    }

    // Penalty for enemy attack near king
    int them=color^1;
    Bitboard kingZone=KING_ATTACKS[kingSq];
    int attackCount=0;
    for(int p=KNIGHT;p<=QUEEN;p++){
        Bitboard bb=b.pieces[them][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            if(b.attacksFrom(sq,p,them)&kingZone) attackCount++;
        }
    }
    score -= attackCount * attackCount * 8;
    return score;
}

Score Evaluator::evalMobility(Board& b, int color){
    Score score=0;
    Bitboard occ=b.occupied[BOTH];
    Bitboard own=b.occupied[color];

    for(int p=KNIGHT;p<=QUEEN;p++){
        Bitboard bb=b.pieces[color][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            int mob=pop(b.attacksFrom(sq,p,color)&~own);
            score += mob * (p==QUEEN?1:p==ROOK?2:3);
        }
    }
    return score;
}

Score Evaluator::evalSide(Board& b, int color, bool endgame){
    Score score=0;
    static const int* PST_TABLE[6][2]={
        {PST::PAWN_MG,   PST::PAWN_MG},   // pawn (same mg/eg)
        {PST::KNIGHT_MG, PST::KNIGHT_MG},
        {PST::BISHOP_MG, PST::BISHOP_MG},
        {PST::ROOK_MG,   PST::ROOK_MG},
        {PST::QUEEN_MG,  PST::QUEEN_MG},
        {PST::KING_MG,   PST::KING_EG}
    };

    for(int p=0;p<6;p++){
        Bitboard bb=b.pieces[color][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            score += PIECE_VALUE[p];
            int pstSq = (color==WHITE) ? PST::mirror(sq) : sq;
            score += PST_TABLE[p][endgame?1:0][pstSq];
        }
    }

    // Bishop pair bonus
    if(pop(b.pieces[color][BISHOP])>=2) score+=30;

    // Rook on open/semi-open file
    Bitboard rooks=b.pieces[color][ROOK];
    while(rooks){
        int sq=lsb3(rooks); rooks&=rooks-1;
        int f=sq&7;
        Bitboard fileBB=0x0101010101010101ULL<<f;
        if(!(b.pieces[color][PAWN]&fileBB)){
            if(!(b.pieces[color^1][PAWN]&fileBB)) score+=20; // open file
            else score+=10; // semi-open
        }
    }

    score += evalPawnStructure(b,color);
    if(!endgame) score += evalKingSafety(b,color);
    score += evalMobility(b,color)/4;

    return score;
}

Score Evaluator::evaluate(Board& b){
    bool eg=isEndgame(b);
    Score w=evalSide(b,WHITE,eg);
    Score bk=evalSide(b,BLACK,eg);
    Score score = w - bk;

    // Tempo bonus for side to move
    score += (b.sideToMove==WHITE) ? 10 : -10;

    // Return from perspective of side to move
    return (b.sideToMove==WHITE) ? score : -score;
}
