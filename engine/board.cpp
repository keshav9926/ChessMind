#include "board.h"
#include <sstream>
#include <cstring>
#include <random>
#include <algorithm>

// ─── Global tables ─────────────────────────────────────────────────────────────
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[2][64];
Bitboard RAY[64][8];
uint64_t ZOBRIST_PIECE[2][6][64];
uint64_t ZOBRIST_CASTLING[16];
uint64_t ZOBRIST_EP[9];
uint64_t ZOBRIST_SIDE;

// ─── Bitboard helpers ──────────────────────────────────────────────────────────
static inline int  lsb(Bitboard b)   { return __builtin_ctzll(b); }
static inline int  popcount(Bitboard b){ return __builtin_popcountll(b); }
static inline Bitboard popLSB(Bitboard& b){ Bitboard t=b&(-b); b&=b-1; return t; }

static Bitboard rankMask(int sq){ return 0xFFULL << (sq & 56); }
static Bitboard fileMask(int sq){ return 0x0101010101010101ULL << (sq & 7); }

// direction offsets: N NE E SE S SW W NW
static const int DIR[8] = { 8, 9, 1, -7, -8, -9, -1, 7 };

static bool onBoard(int sq, int dir) {
    int f = sq&7, r = sq>>3;
    switch(dir){
        case  8: return r<7;
        case  9: return r<7 && f<7;
        case  1: return f<7;
        case -7: return r>0 && f<7;
        case -8: return r>0;
        case -9: return r>0 && f>0;
        case -1: return f>0;
        case  7: return r<7 && f>0;
    }
    return false;
}

void Board::initTables() {
    // Zobrist keys
    std::mt19937_64 rng(0xDEADBEEFCAFEBABEULL);
    for(int c=0;c<2;c++) for(int p=0;p<6;p++) for(int s=0;s<64;s++)
        ZOBRIST_PIECE[c][p][s] = rng();
    for(int i=0;i<16;i++) ZOBRIST_CASTLING[i] = rng();
    for(int i=0;i<9;i++)  ZOBRIST_EP[i]       = rng();
    ZOBRIST_SIDE = rng();

    // Knight attacks
    for(int sq=0;sq<64;sq++){
        Bitboard b=0; int f=sq&7, r=sq>>3;
        int kd[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for(auto& d:kd){ int nf=f+d[0],nr=r+d[1]; if(nf>=0&&nf<8&&nr>=0&&nr<8) b|=1ULL<<(nr*8+nf); }
        KNIGHT_ATTACKS[sq]=b;
    }
    // King attacks
    for(int sq=0;sq<64;sq++){
        Bitboard b=0;
        for(int d=0;d<8;d++) if(onBoard(sq,DIR[d])) b|=1ULL<<(sq+DIR[d]);
        KING_ATTACKS[sq]=b;
    }
    // Pawn attacks
    for(int sq=0;sq<64;sq++){
        int f=sq&7, r=sq>>3;
        Bitboard w=0,bk=0;
        if(r<7){ if(f>0) w|=1ULL<<(sq+7); if(f<7) w|=1ULL<<(sq+9); }
        if(r>0){ if(f>0) bk|=1ULL<<(sq-9); if(f<7) bk|=1ULL<<(sq-7); }
        PAWN_ATTACKS[WHITE][sq]=w; PAWN_ATTACKS[BLACK][sq]=bk;
    }
    // Rays
    for(int sq=0;sq<64;sq++){
        for(int d=0;d<8;d++){
            Bitboard b=0; int cur=sq;
            while(onBoard(cur,DIR[d])){ cur+=DIR[d]; b|=1ULL<<cur; }
            RAY[sq][d]=b;
        }
    }
}

void initAll(){ Board::initTables(); }

// ─── Sliding attacks (classical) ──────────────────────────────────────────────
// DIR = { N(+8), NE(+9), E(+1), SE(-7), S(-8), SW(-9), W(-1), NW(+7) }
// Positive dirs (squares increase): 0,1,2,7 → use LSB of blockers
// Negative dirs (squares decrease): 3,4,5,6 → use MSB of blockers
static inline int firstBlocker(Bitboard blk, int dir){
    // positive directions: N(0),NE(1),E(2),NW(7)
    if(dir==0||dir==1||dir==2||dir==7) return __builtin_ctzll(blk);
    else return 63-__builtin_clzll(blk);
}

static Bitboard rookAttacks(int sq, Bitboard occ){
    Bitboard atk=0;
    for(int d:{0,2,4,6}){
        Bitboard ray=RAY[sq][d];
        Bitboard blk=ray&occ;
        if(blk){ int b=firstBlocker(blk,d); ray^=RAY[b][d]; }
        atk|=ray;
    }
    return atk;
}
static Bitboard bishopAttacks(int sq, Bitboard occ){
    Bitboard atk=0;
    for(int d:{1,3,5,7}){
        Bitboard ray=RAY[sq][d];
        Bitboard blk=ray&occ;
        if(blk){ int b=firstBlocker(blk,d); ray^=RAY[b][d]; }
        atk|=ray;
    }
    return atk;
}

// ─── Attack generation ─────────────────────────────────────────────────────────
Bitboard Board::attacksFrom(int sq, int piece, int color) const {
    Bitboard occ = occupied[BOTH];
    switch(piece){
        case PAWN:   return PAWN_ATTACKS[color][sq];
        case KNIGHT: return KNIGHT_ATTACKS[sq];
        case BISHOP: return bishopAttacks(sq,occ);
        case ROOK:   return rookAttacks(sq,occ);
        case QUEEN:  return bishopAttacks(sq,occ)|rookAttacks(sq,occ);
        case KING:   return KING_ATTACKS[sq];
    }
    return 0;
}

bool Board::isSquareAttacked(int sq, int attacker) const {
    Bitboard occ=occupied[BOTH];
    if(PAWN_ATTACKS[attacker^1][sq] & pieces[attacker][PAWN])   return true;
    if(KNIGHT_ATTACKS[sq]           & pieces[attacker][KNIGHT]) return true;
    if(KING_ATTACKS[sq]             & pieces[attacker][KING])   return true;
    if(bishopAttacks(sq,occ) & (pieces[attacker][BISHOP]|pieces[attacker][QUEEN])) return true;
    if(rookAttacks(sq,occ)   & (pieces[attacker][ROOK]  |pieces[attacker][QUEEN])) return true;
    return false;
}

bool Board::isInCheck(int color) const {
    return isSquareAttacked(kingSquare(color), color^1);
}

int Board::kingSquare(int color) const {
    return lsb(pieces[color][KING]);
}

// ─── Piece placement helpers ───────────────────────────────────────────────────
void Board::putPiece(int c, int p, int sq){
    pieces[c][p] |= 1ULL<<sq; occupied[c] |= 1ULL<<sq;
    occupied[BOTH]|=1ULL<<sq; board[sq]=p; colorOn[sq]=c;
    hash ^= ZOBRIST_PIECE[c][p][sq];
}
void Board::removePiece(int c, int p, int sq){
    pieces[c][p] &=~(1ULL<<sq); occupied[c] &=~(1ULL<<sq);
    occupied[BOTH]&=~(1ULL<<sq); board[sq]=-1; colorOn[sq]=-1;
    hash ^= ZOBRIST_PIECE[c][p][sq];
}
void Board::movePiece(int c, int p, int from, int to){
    removePiece(c,p,from); putPiece(c,p,to);
}


// ─── FEN parsing ──────────────────────────────────────────────────────────────
void Board::setFen(const std::string& fen){
    memset(pieces,0,sizeof(pieces));
    memset(occupied,0,sizeof(occupied));
    memset(board,-1,sizeof(board));
    memset(colorOn,-1,sizeof(colorOn));
    hash=0; history.clear();

    std::istringstream ss(fen);
    std::string pos,side,castle,ep; int hmc,fm;
    ss>>pos>>side>>castle>>ep>>hmc>>fm;

    // position
    int sq=56;
    for(char c:pos){
        if(c=='/'){sq-=16;}
        else if(c>='1'&&c<='8'){sq+=c-'0';}
        else{
            int color=(c>='a'&&c<='z')?BLACK:WHITE;
            char l=tolower(c);
            int p = (l=='p')?PAWN:(l=='n')?KNIGHT:(l=='b')?BISHOP:
                    (l=='r')?ROOK:(l=='q')?QUEEN:KING;
            putPiece(color,p,sq++);
        }
    }
    sideToMove=(side=="w")?WHITE:BLACK;
    if(sideToMove==BLACK) hash^=ZOBRIST_SIDE;

    castlingRights=0;
    if(castle.find('K')!=std::string::npos) castlingRights|=WK_CASTLE;
    if(castle.find('Q')!=std::string::npos) castlingRights|=WQ_CASTLE;
    if(castle.find('k')!=std::string::npos) castlingRights|=BK_CASTLE;
    if(castle.find('q')!=std::string::npos) castlingRights|=BQ_CASTLE;
    hash^=ZOBRIST_CASTLING[castlingRights];

    epSquare=NO_SQ;
    if(ep!="-"){ int f=ep[0]-'a', r=ep[1]-'1'; epSquare=r*8+f; hash^=ZOBRIST_EP[f]; }
    else hash^=ZOBRIST_EP[8];
    halfMoveClock=hmc; fullMove=fm;
}

std::string Board::toFen() const {
    static const char PIECE_CHAR[2][7]={{'P','N','B','R','Q','K','.'},
                                        {'p','n','b','r','q','k','.'}};
    std::string fen;
    for(int r=7;r>=0;r--){
        int empty=0;
        for(int f=0;f<8;f++){
            int sq=r*8+f;
            if(board[sq]==-1){ empty++; }
            else{ if(empty){fen+=(char)('0'+empty);empty=0;} fen+=PIECE_CHAR[colorOn[sq]][board[sq]]; }
        }
        if(empty) fen+=(char)('0'+empty);
        if(r>0) fen+='/';
    }
    fen+=(sideToMove==WHITE)?" w ":" b ";
    std::string c;
    if(castlingRights&WK_CASTLE) c+='K';
    if(castlingRights&WQ_CASTLE) c+='Q';
    if(castlingRights&BK_CASTLE) c+='k';
    if(castlingRights&BQ_CASTLE) c+='q';
    if(c.empty()) c="-";
    fen+=c+" ";
    if(epSquare==NO_SQ) fen+="-";
    else{ fen+=(char)('a'+(epSquare&7)); fen+=(char)('1'+(epSquare>>3)); }
    fen+=" "+std::to_string(halfMoveClock)+" "+std::to_string(fullMove);
    return fen;
}

// ─── Make / Undo move ──────────────────────────────────────────────────────────
void Board::makeMove(Move mv){
    StateInfo st;
    st.castlingRights=castlingRights; st.epSquare=epSquare;
    st.halfMoveClock=halfMoveClock;   st.hash=hash; st.lastMove=mv;
    st.capturedPiece=-1;
    history.push_back(st);

    int from=moveFrom(mv), to=moveTo(mv), pc=getMvPiece(mv);
    int cap=moveCap(mv), promo=movePromo(mv);
    int us=sideToMove, them=us^1;

    // Remove ep/castling from hash
    hash^=ZOBRIST_CASTLING[castlingRights];
    if(epSquare!=NO_SQ) hash^=ZOBRIST_EP[epSquare&7]; else hash^=ZOBRIST_EP[8];

    halfMoveClock++;
    if(pc==PAWN||cap!=6) halfMoveClock=0;

    // Capture
    if(cap!=6 && !moveEP(mv)){ removePiece(them,cap,to); history.back().capturedPiece=cap; }

    // En passant capture
    if(moveEP(mv)){ int epCap=to+(them==WHITE?8:-8); removePiece(them,PAWN,epCap); history.back().capturedPiece=PAWN; }

    // Move piece
    if(promo!=6){ removePiece(us,PAWN,from); putPiece(us,promo,to); }
    else this->movePiece(us,pc,from,to);

    // Castling rook
    if(moveCastle(mv)){
        if(to==G1) this->movePiece(WHITE,ROOK,H1,F1);
        else if(to==C1) this->movePiece(WHITE,ROOK,A1,D1);
        else if(to==G8) this->movePiece(BLACK,ROOK,H8,F8);
        else if(to==C8) this->movePiece(BLACK,ROOK,A8,D8);
    }

    // Update castling rights
    static const int CR_MASK[64]={
        ~WQ_CASTLE,15,15,15,~(WK_CASTLE|WQ_CASTLE),15,15,~WK_CASTLE,
        15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,
        ~BQ_CASTLE,15,15,15,~(BK_CASTLE|BQ_CASTLE),15,15,~BK_CASTLE
    };
    castlingRights &= CR_MASK[from] & CR_MASK[to];

    // Update EP square
    epSquare=NO_SQ;
    if(moveDPP(mv)) epSquare=to+(them==WHITE?8:-8);

    // Restore hash with new state
    hash^=ZOBRIST_CASTLING[castlingRights];
    if(epSquare!=NO_SQ) hash^=ZOBRIST_EP[epSquare&7]; else hash^=ZOBRIST_EP[8];

    sideToMove=them;
    hash^=ZOBRIST_SIDE;
    if(us==BLACK) fullMove++;
}

void Board::undoMove(Move mv){
    StateInfo& st=history.back();
    int from=moveFrom(mv), to=moveTo(mv), pc=getMvPiece(mv);
    int cap=st.capturedPiece, promo=movePromo(mv);

    sideToMove^=1;
    int us=sideToMove, them=us^1;

    // Undo promotion
    if(promo!=6){ removePiece(us,promo,to); putPiece(us,PAWN,from); }
    else this->movePiece(us,pc,to,from);

    // Restore capture
    if(cap!=-1 && !moveEP(mv)) putPiece(them,cap,to);
    if(moveEP(mv)){ int epCap=to+(them==WHITE?8:-8); putPiece(them,PAWN,epCap); }

    // Undo castling rook
    if(moveCastle(mv)){
        if(to==G1) this->movePiece(WHITE,ROOK,F1,H1);
        else if(to==C1) this->movePiece(WHITE,ROOK,D1,A1);
        else if(to==G8) this->movePiece(BLACK,ROOK,F8,H8);
        else if(to==C8) this->movePiece(BLACK,ROOK,D8,A8);
    }

    castlingRights=st.castlingRights;
    epSquare=st.epSquare;
    halfMoveClock=st.halfMoveClock;
    hash=st.hash;
    if(us==BLACK) fullMove--;
    history.pop_back();
}

void Board::makeNullMove(){
    StateInfo st; st.castlingRights=castlingRights; st.epSquare=epSquare;
    st.halfMoveClock=halfMoveClock; st.hash=hash; st.lastMove=0; st.capturedPiece=-1;
    history.push_back(st);
    hash^=ZOBRIST_SIDE;
    if(epSquare!=NO_SQ){ hash^=ZOBRIST_EP[epSquare&7]; hash^=ZOBRIST_EP[8]; }
    epSquare=NO_SQ;
    sideToMove^=1;
}

void Board::undoNullMove(){
    StateInfo& st=history.back();
    sideToMove^=1; castlingRights=st.castlingRights;
    epSquare=st.epSquare; halfMoveClock=st.halfMoveClock; hash=st.hash;
    history.pop_back();
}

bool Board::isRepetition() const {
    int cnt=0;
    for(int i=(int)history.size()-2;i>=0;i-=2){
        if(history[i].hash==hash){ if(++cnt>=2) return true; }
        if(history[i].halfMoveClock==0) break;
    }
    return false;
}

bool Board::isInsufficientMaterial() const {
    int total=popcount(occupied[BOTH]);
    if(total==2) return true;
    if(total==3 && (pieces[WHITE][KNIGHT]||pieces[BLACK][KNIGHT]||
                    pieces[WHITE][BISHOP]||pieces[BLACK][BISHOP])) return true;
    return false;
}

uint64_t Board::computeHash() const {
    uint64_t h=0;
    for(int c=0;c<2;c++) for(int p=0;p<6;p++){
        Bitboard b=pieces[c][p];
        while(b){ int sq=lsb(b); h^=ZOBRIST_PIECE[c][p][sq]; b&=b-1; }
    }
    h^=ZOBRIST_CASTLING[castlingRights];
    if(epSquare!=NO_SQ) h^=ZOBRIST_EP[epSquare&7]; else h^=ZOBRIST_EP[8];
    if(sideToMove==BLACK) h^=ZOBRIST_SIDE;
    return h;
}
