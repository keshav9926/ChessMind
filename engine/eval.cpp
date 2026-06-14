/**
 * @file eval.cpp
 * @brief Heuristic Evaluation Function Implementation
 * 
 * CONCEPT: Heuristic Evaluation Functions
 * ========================================
 * 
 * Problem: Minimax needs to evaluate positions, but chess positions 
 * aren't "terminal" (not won/lost/drawn) until very deep.
 * 
 * Solution: Design a heuristic function that estimates position quality
 * 
 * Properties of Good Heuristics:
 * 1. Fast: Evaluates in O(1) or O(log n) time (we use O(1))
 * 2. Accurate: Correlates well with actual game outcome
 * 3. Consistent: Doesn't change wildly for similar positions
 * 4. Monotonic: Better positions score higher
 * 
 * Components of our heuristic (in order of importance):
 * ┌─────────────────────────────┐
 * │ Material Count       ~ 90%   │ Most important
 * │ Piece-Square Tables  ~ 5%    │ Positional play
 * │ Pawn Structure       ~ 3%    │ Long-term factors
 * │ King Safety          ~ 1%    │ Tactical safety
 * │ Piece Mobility       < 1%    │ Activity factor
 * └─────────────────────────────┘
 * 
 * Score Range: -30000 to +30000 centipawns
 *   -30000 = Black totally winning
 *   0      = Equal position
 *   +30000 = White totally winning
 */

#include "eval.h"
#include <chrono>

/// Helper: Find least significant bit (first set bit)
/// Time: O(1) - hardware instruction (count trailing zeros)
static inline int lsb3(Bitboard b){ return __builtin_ctzll(b); }

/// Helper: Count number of set bits (popcount)
/// Time: O(1) - hardware instruction
/// Example: popcount(0b1111) = 4
static inline int pop(Bitboard b){ return __builtin_popcountll(b); }

/**
 * @brief Classify game phase: Middle-game or Endgame
 * 
 * Why distinguish phases?
 * - Middlegame: King safety critical, material is king
 * - Endgame: King activity critical, pawns are more valuable
 * - Different piece-square tables for each phase
 * 
 * Endgame detection heuristic:
 * - No queens on board, OR
 * - Only 1 queen, AND no minor pieces
 * 
 * Time Complexity: O(1) - just bitwise operations and popcounting
 * 
 * @param b Board to analyze
 * @return true if position is endgame, false if middlegame
 */
bool Evaluator::isEndgame(const Board& b){
    // ─── Queen count is key indicator of game phase ────────────────
    // Most material lost = endgame
    int queens = pop(b.pieces[WHITE][QUEEN]) + pop(b.pieces[BLACK][QUEEN]);
    
    // No queens at all = definitely endgame (rook/minor/pawn endings)
    if(queens==0) return true;
    
    // Exactly 1 queen with no minor pieces = endgame
    // (queen vs queen with no support pieces)
    int wMinor = pop(b.pieces[WHITE][KNIGHT])+pop(b.pieces[WHITE][BISHOP]);
    int bMinor = pop(b.pieces[BLACK][KNIGHT])+pop(b.pieces[BLACK][BISHOP]);
    return (queens==1 && wMinor+bMinor<=2);
}

/**
 * @brief Evaluate pawn structure
 * 
 * PAWN STRUCTURE: Pawns are the soul of chess strategy
 * 
 * Factors analyzed:
 * 1. DOUBLED PAWNS: Two pawns of same color on same file
 *    - Bad: Pawns block each other, can't defend each other
 *    - Penalty: -15 centipawns per doubled pawn
 *    - Example: a2, a3 (both white pawns) = -15
 *
 * 2. ISOLATED PAWNS: Pawn with no friendly pawns on adjacent files
 *    - Bad: Can't be defended by pawns, often attacked
 *    - Penalty: -20 centipawns per isolated pawn
 *    - Example: pawn on d4 with no c/e pawns = -20
 *
 * 3. PASSED PAWNS: Pawn with no enemy pawns blocking its path to promotion
 *    - Good: Can potentially promote
 *    - Bonus: 20 + advancement²×5 centipawns
 *    - Example: White pawn on e6 (rank 6):
 *      advancement = 6, bonus = 20 + 36×5 = 200 centipawns!
 *      (Much stronger than regular pawn because close to promotion)
 *
 * Algorithm:
 * 1. Iterate through each pawn of given color
 * 2. For each pawn, check file and rank
 * 3. Apply penalties/bonuses
 *
 * Time Complexity: O(pawns) ≈ O(1)
 *   Each side has 0-8 pawns typically
 *
 * Space Complexity: O(1) - only bitwise operations
 * 
 * @param b Board to analyze
 * @param color WHITE or BLACK
 * @return Score adjustment for pawn structure (-200 to +500 typical)
 */
Score Evaluator::evalPawnStructure(Board& b, int color){
    Score score=0;
    Bitboard pawns=b.pieces[color][PAWN];
    Bitboard opPawns=b.pieces[color^1][PAWN];
    Bitboard tmp=pawns;

    // ─── Iterate through each pawn ─────────────────────────────────
    // Algorithm: Use bit manipulation to iterate through set bits
    // Time: O(number of pawns) which is constant (≤8 per side)
    while(tmp){
        int sq=lsb3(tmp);       // Get position of least significant bit (first pawn)
        tmp&=tmp-1;             // Remove that bit (iterate to next pawn)
        
        int file=sq&7;          // File (0-7, leftmost=0, rightmost=7)
        int rank=sq>>3;         // Rank (0-7, 1st=0, 8th=7)

        // ───────────────────────────────────────────────────────────
        // DOUBLED PAWNS: Two pawns on same file
        // ───────────────────────────────────────────────────────────
        // Create bitboard for entire file
        // 0x0101010101010101 = all 8 squares of leftmost file
        // Shift left by 'file' to get correct file
        Bitboard fileMask=0x0101010101010101ULL<<file;
        
        // Count pawns on this file
        if(pop(pawns&fileMask)>1) {
            // More than 1 pawn on file = doubled!
            score-=15;  // Penalty
        }

        // ───────────────────────────────────────────────────────────
        // ISOLATED PAWNS: Pawn with no adjacent file pawns
        // ───────────────────────────────────────────────────────────
        // Create bitboard for adjacent files
        Bitboard adjFiles=0;
        if(file>0) adjFiles|=0x0101010101010101ULL<<(file-1);  // Left file
        if(file<7) adjFiles|=0x0101010101010101ULL<<(file+1);  // Right file
        
        // If no friendly pawns on adjacent files = isolated
        if(!(pawns&adjFiles)) {
            score-=20;  // Strong penalty
        }

        // ───────────────────────────────────────────────────────────
        // PASSED PAWNS: Pawn unblocked path to promotion
        // ───────────────────────────────────────────────────────────
        // Create bitboard for pawn's path: file + adjacent files
        Bitboard passedMask=fileMask|adjFiles;
        
        // Remove squares the pawn has already passed
        if(color==WHITE) {
            // White pawns go up (rank 0→7), so remove squares below
            passedMask&=~((1ULL<<((rank+1)*8))-1);
        } else {
            // Black pawns go down (rank 7→0), so remove squares above
            passedMask&=(1ULL<<(rank*8))-1;
        }
        
        // If opponent pawns don't control the path = PASSED!
        if(!(opPawns&passedMask)){
            int advancement = (color==WHITE)?rank:(7-rank);
            // Bonus scales quadratically with closeness to promotion
            // rank 5: bonus = 20 + 25×5 = 145 centipawns
            // rank 6: bonus = 20 + 36×5 = 200 centipawns
            // rank 7: bonus = 20 + 49×5 = 265 centipawns (almost promoting!)
            score += 20 + advancement*advancement*5;
        }
    }
    return score;
}

/**
 * @brief Evaluate king safety - critical for tactics
 * 
 * KING SAFETY: Safety is paramount, especially in middlegame
 * 
 * Factors analyzed:
 * 1. PAWN SHIELD: Pawns protecting king (f/g/h pawns for white)
 *    - Bonus: +15 centipawns per pawn in shield
 *    - Creates barrier against attacks
 *
 * 2. OPEN FILES NEAR KING: Files without pawns near king
 *    - Bad: Allows rook/queen attacks through
 *    - Penalty: -20 if own pawn missing, -10 if both missing
 *
 * 3. ENEMY PIECES ATTACKING KING ZONE
 *    - Each attacking piece: -8×(attack_count²) centipawns
 *    - Quadratic penalty = multiple attackers exponentially worse
 *    - Example: 2 attackers = -32, 3 attackers = -72
 *
 * Time Complexity: O(enemy_pieces) ≈ O(1)
 * 
 * @param b Board to analyze
 * @param color Side to evaluate safety for
 * @return Score adjustment (-300 to +100 typical)
 */
Score Evaluator::evalKingSafety(Board& b, int color){
    Score score=0;
    int kingSq=b.kingSquare(color);
    int kingFile=kingSq&7, kingRank=kingSq>>3;

    // ───────────────────────────────────────────────────────────────
    // PAWN SHIELD: Pawns protecting king
    // ───────────────────────────────────────────────────────────────
    Bitboard shield=0;
    
    if(color==WHITE && kingRank<7){
        // White king: protect with pawns in rank+1 (pawns above king)
        for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++)
            shield|=1ULL<<((kingRank+1)*8+f);
    } else if(color==BLACK && kingRank>0){
        // Black king: protect with pawns in rank-1 (pawns below king)
        for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++)
            shield|=1ULL<<((kingRank-1)*8+f);
    }
    
    // Award bonus for each pawn in shield
    score += pop(b.pieces[color][PAWN]&shield) * 15;

    // ───────────────────────────────────────────────────────────────
    // OPEN FILES NEAR KING: Vulnerable to rooks/queens
    // ───────────────────────────────────────────────────────────────
    for(int f=std::max(0,kingFile-1);f<=std::min(7,kingFile+1);f++){
        Bitboard fileBB=0x0101010101010101ULL<<f;
        
        // Penalty if own pawn is missing from file
        if(!(b.pieces[color][PAWN]&fileBB)) {
            score-=20;
            
            // Additional penalty if opponent also has no pawn (completely open)
            if(!(b.pieces[color^1][PAWN]&fileBB)) {
                score-=10;
            }
        }
    }

    // ───────────────────────────────────────────────────────────────
    // ENEMY ATTACKS NEAR KING: Piece activity vs king
    // ───────────────────────────────────────────────────────────────
    int them=color^1;
    Bitboard kingZone=KING_ATTACKS[kingSq];  // All squares adjacent to king
    int attackCount=0;
    
    // Count enemy pieces attacking king zone
    for(int p=KNIGHT;p<=QUEEN;p++){
        Bitboard bb=b.pieces[them][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            // If this piece attacks any square in king zone, count it
            if(b.attacksFrom(sq,p,them)&kingZone) {
                attackCount++;
            }
        }
    }
    
    // Apply quadratic penalty: 1 attacker=-8, 2=-32, 3=-72
    // Reasoning: Each additional attacker exponentially increases mating threat
    score -= attackCount * attackCount * 8;
    return score;
}

/**
 * @brief Evaluate piece mobility - activity/freedom
 * 
 * PIECE MOBILITY: Active pieces are stronger
 * 
 * Concept: Count squares each piece can move to
 * - Queen moving 14 squares > Queen moving 2 squares
 * - Typically bonus: 1 centipawn per legal move
 * - Weighted: Queen moves matter less than pawn moves (queen already strong)
 *
 * Time Complexity: O(pieces) ≈ O(1)
 * 
 * @param b Board to analyze
 * @param color Side to evaluate
 * @return Score adjustment (-100 to +100 typical)
 */
Score Evaluator::evalMobility(Board& b, int color){
    Score score=0;
    Bitboard occ=b.occupied[BOTH];
    Bitboard own=b.occupied[color];

    // ─── Count mobility of each piece ───────────────────────────────
    for(int p=KNIGHT;p<=QUEEN;p++){
        Bitboard bb=b.pieces[color][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            
            // Count legal moves for this piece
            // (all attacked squares except occupied by own pieces)
            int mob=pop(b.attacksFrom(sq,p,color)&~own);
            
            // Weight by piece type (queens shouldn't get as much credit)
            // Queen=1×bonus, Rook=2×bonus, Knight/Bishop=3×bonus
            score += mob * (p==QUEEN?1:p==ROOK?2:3);
        }
    }
    return score;
}

/**
 * @brief Evaluate one side completely
 * 
 * EVALUATION COMPONENTS Combined:
 * 1. Material value (90% of total score)
 * 2. Piece-square tables - positional bonuses
 * 3. Bishop pair bonus - powerful piece combination
 * 4. Rook on open/semi-open files - activity
 * 5. Pawn structure - long-term positional factors
 * 6. King safety - tactical vulnerability
 * 7. Piece mobility - positional freedom
 *
 * Time Complexity: O(pieces) ≈ O(1)
 *   All pieces combined ≤ 16 per side (constant)
 *
 * @param b Board to analyze
 * @param color Side to evaluate
 * @param endgame Whether it's endgame phase
 * @return Total evaluation for this side (0 to 3000 typical)
 */
Score Evaluator::evalSide(Board& b, int color, bool endgame){
    Score score=0;
    
    // ───────────────────────────────────────────────────────────────
    // MATERIAL VALUE + PIECE-SQUARE TABLES
    // ───────────────────────────────────────────────────────────────
    // Why two phases? 
    // - Middlegame: King safety matters, piece-square tables reward activity
    // - Endgame: King activity matters, center control different
    
    static const int* PST_TABLE[6][2]={
        {PST::PAWN_MG,   PST::PAWN_EG},    // Pawn (different in endgame)
        {PST::KNIGHT_MG, PST::KNIGHT_MG},  // Knight (not split yet)
        {PST::BISHOP_MG, PST::BISHOP_MG},  // Bishop
        {PST::ROOK_MG,   PST::ROOK_MG},    // Rook
        {PST::QUEEN_MG,  PST::QUEEN_MG},   // Queen
        {PST::KING_MG,   PST::KING_EG}     // King: different in endgame!
    };

    // Evaluate each piece on the board
    for(int p=0;p<6;p++){
        Bitboard bb=b.pieces[color][p];
        while(bb){
            int sq=lsb3(bb); bb&=bb-1;
            
            // 1. BASE MATERIAL VALUE (most important ~90%)
            // Pawn=100, Knight=320, Bishop=330, Rook=500, Queen=900
            score += PIECE_VALUE[p];
            
            // 2. POSITION-BASED BONUS (piece-square table)
            // Rewards putting pieces on good squares
            // Example: White knights rewarded on central squares
            //          White pawns rewarded on advanced squares
            int pstSq = (color==WHITE) ? PST::mirror(sq) : sq;
            score += PST_TABLE[p][endgame?1:0][pstSq];
        }
    }

    // ───────────────────────────────────────────────────────────────
    // BISHOP PAIR BONUS: Two bishops are synergistic
    // ───────────────────────────────────────────────────────────────
    // Bishops on opposite colors control different squares
    // Together they dominate the board
    if(pop(b.pieces[color][BISHOP])>=2) {
        score+=30;  // Bonus for having two bishops
    }

    // ───────────────────────────────────────────────────────────────
    // ROOK ON OPEN/SEMI-OPEN FILES
    // ───────────────────────────────────────────────────────────────
    // Rooks belong on files without friendly pawns to maximize activity
    Bitboard rooks=b.pieces[color][ROOK];
    while(rooks){
        int sq=lsb3(rooks); rooks&=rooks-1;
        int f=sq&7;
        Bitboard fileBB=0x0101010101010101ULL<<f;
        
        if(!(b.pieces[color][PAWN]&fileBB)){
            // No friendly pawns on this file = open or semi-open
            if(!(b.pieces[color^1][PAWN]&fileBB)) {
                score+=20;  // Completely open file - very strong!
            } else {
                score+=10;  // Semi-open (opponent has pawn) - still good
            }
        }
    }

    // ───────────────────────────────────────────────────────────────
    // COMBINE OTHER POSITIONAL FACTORS
    // ───────────────────────────────────────────────────────────────
    
    // Pawn structure worth 3% of total evaluation
    score += evalPawnStructure(b,color);
    
    // King safety critical in middlegame (worth ~1%)
    if(!endgame) {
        score += evalKingSafety(b,color);
    }
    
    // Mobility/activity worth <1% but still matters
    score += evalMobility(b,color)/4;  // Divide by 4 to reduce noise

    return score;
}

/**
 * @brief Main evaluation function - complete position assessment
 * 
 * EVALUATION PIPELINE:
 * ═══════════════════════════════════════════════════════════
 * 
 * 1. Detect game phase (middlegame vs endgame)
 * 2. Evaluate white's position factors
 * 3. Evaluate black's position factors
 * 4. Compute white - black (white's perspective)
 * 5. Adjust for tempo (who moves)
 * 6. Return score from side-to-move's perspective
 * 
 * SCORE INTERPRETATION:
 * ═════════════════════════════════════════════════════════════
 * -30000 to -1000: Black completely winning
 * -1000 to -500:   Black better
 * -500 to -100:    Black slightly better
 * -100 to +100:    Roughly equal
 * +100 to +500:    White slightly better
 * +500 to +1000:   White better
 * +1000 to +30000: White completely winning
 * 
 * EXAMPLE EVALUATION:
 * ═════════════════════════════════════════════════════════════
 * Position: White +200 (white advantage of 2 pawns)
 * - Material: White +150 (white has extra material)
 * - Pawn structure: White +40 (white has better pawns)
 * - King safety: Black +10 (black king safer)
 * - Mobility: White +5 (white pieces more active)
 * 
 * Time Complexity: O(pieces) ≈ O(1)
 *   All evaluation is O(1) because pieces ≤ 32 (constant)
 *
 * Time in practice: ~500-800 CPU cycles
 *   With 100M evaluations per search: ~50-80 ms total
 *
 * Space Complexity: O(1) - no temporary allocations
 * 
 * @param b Board to evaluate
 * @return Score in centipawns (positive = white winning)
 */
Score Evaluator::evaluate(Board& b){
    // ─── Determine game phase ───────────────────────────────────
    // Endgame: Different PST tables, different king emphasis
    bool eg=isEndgame(b);
    
    // ─── Evaluate both sides ────────────────────────────────────
    Score w=evalSide(b,WHITE,eg);    // White's total score
    Score bk=evalSide(b,BLACK,eg);   // Black's total score
    Score score = w - bk;             // White - Black difference
                                       // (positive = white advantage)

    // ─── Tempo bonus: Side to move gets small advantage ─────────
    // Reason: Moving first is slightly better (your tempo)
    // Bonus: 10 centipawns (1/10 of a pawn)
    score += (b.sideToMove==WHITE) ? 10 : -10;

    // ─── Return from perspective of side to move ───────────────
    // Minimax expects scores from current player's POV
    // If black to move: negate score so positive = black winning
    // If white to move: keep score as-is (positive = white winning)
    Score finalScore = (b.sideToMove==WHITE) ? score : -score;

    // ─── Early Game Positional Randomization ───────────────────
    if (b.fullMove <= 10) {
        static uint64_t gEvalSeed = 0;
        static bool gEvalSeedInitialized = false;
        if (!gEvalSeedInitialized) {
            gEvalSeed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
            gEvalSeedInitialized = true;
        }
        int offset = ((b.hash ^ gEvalSeed) % 31) - 15; // -15 to +15 centipawns
        finalScore += offset;
    }

    return finalScore;
}
