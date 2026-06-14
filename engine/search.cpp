#include "search.h"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <iostream>

static inline long long elapsed_ms(std::chrono::steady_clock::time_point t0){
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-t0).count();
}

bool Search::timeUp() const {
    if((info.nodes & 2047) != 0) return false;
    return elapsed_ms(startTime) >= timeLimitMs;
}

int Search::mvvLva(int victim, int attacker) const {
    return PIECE_VALUE[victim]*10 - PIECE_VALUE[attacker];
}

int Search::scoreMove(Board& b, Move m, int ply, Move ttMove) const {
    if(m==ttMove) return 2000000;
    if(movePromo(m)!=6) return 1500000 + PIECE_VALUE[movePromo(m)];
    if(moveIsCapture(m)) return 1000000 + mvvLva(moveCap(m), getMvPiece(m));
    if(killerMoves[ply][0]==m) return 900000;
    if(killerMoves[ply][1]==m) return 890000;
    return historyTable[b.sideToMove][getMvPiece(m)][moveTo(m)];
}

void Search::orderMoves(Board& b, std::vector<Move>& moves, int ply, Move ttMove){
    std::sort(moves.begin(), moves.end(), [&](Move a, Move bm){
        return scoreMove(b,a,ply,ttMove) > scoreMove(b,bm,ply,ttMove);
    });
}

Score Search::quiescence(Board& b, Score alpha, Score beta, int ply){
    info.nodes++;
    if(ply > info.seldepth) info.seldepth = ply;
    if(b.isRepetition() || b.halfMoveClock>=100) return DRAW_SCORE;

    Score stand_pat = Evaluator::evaluate(b);
    if(stand_pat >= beta) return beta;
    if(stand_pat > alpha) alpha = stand_pat;
    if(ply >= MAX_PLY)    return stand_pat;

    auto captures = MoveGen::generateCaptures(b);
    orderMoves(b, captures, ply, 0);

    for(Move m : captures){
        if(stop || timeUp()){ stop=true; break; }
        if(stand_pat + PIECE_VALUE[moveCap(m)] + 200 < alpha && movePromo(m) == 6) continue;
        b.makeMove(m);
        Score score = -quiescence(b, -beta, -alpha, ply+1);
        b.undoMove(m);
        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }
    return alpha;
}

Score Search::alphaBeta(Board& b, int depth, int ply, Score alpha, Score beta, bool nullOk){
    info.nodes++;
    if(stop || timeUp()){ stop=true; return 0; }

    if(ply > 0 && (b.isRepetition() || b.halfMoveClock>=100 || b.isInsufficientMaterial()))
        return DRAW_SCORE;
    if(ply >= MAX_PLY) return Evaluator::evaluate(b);

    Score mateVal = MATE_SCORE - ply;
    if(alpha < -mateVal) alpha = -mateVal;
    if(beta  >  mateVal) beta  =  mateVal;
    if(alpha >= beta) return alpha;

    TTEntry* tte = tt.probe(b.hash);
    Move ttMove = tte ? tte->bestMove : 0;
    if(tte && tte->depth >= depth && ply > 0){
        Score s = tte->score;
        if(tte->flag == TT_EXACT) return s;
        if(tte->flag == TT_LOWER && s >= beta)  return s;
        if(tte->flag == TT_UPPER && s <= alpha) return s;
    }

    bool inCheck = b.isInCheck(b.sideToMove);
    if(inCheck) depth++;
    if(depth <= 0) return quiescence(b, alpha, beta, ply);

    bool isPV = (beta - alpha > 1);

    // Null move pruning
    if(nullOk && !isPV && !inCheck && depth >= 3 && !Evaluator::isEndgame(b)){
        int R = (depth >= 6) ? 3 : 2;
        b.makeNullMove();
        Score ns = -alphaBeta(b, depth-1-R, ply+1, -beta, -beta+1, false);
        b.undoNullMove();
        if(ns >= beta){
            info.pruned++;
            if(ns >= MATE_SCORE - MAX_PLY) ns = beta;
            return ns;
        }
    }

    // Reverse futility pruning
    Score staticEval = 0;
    if(depth <= 3) {
        staticEval = Evaluator::evaluate(b);
    }
    if(!isPV && !inCheck && depth <= 3){
        if(staticEval - 120*depth >= beta) return staticEval - 120*depth;
    }

    auto moves = MoveGen::generateLegal(b);
    if(moves.empty()) return inCheck ? (-MATE_SCORE + ply) : DRAW_SCORE;

    orderMoves(b, moves, ply, ttMove);

    Score bestScore = -INF;
    Move  bestMove  = 0;
    TTFlag flag     = TT_UPPER;
    int   searched  = 0;

    for(Move m : moves){
        // Futility pruning at depth 1 (skip quiet moves if position is bad)
        if(depth == 1 && !isPV && !inCheck && !moveIsCapture(m) && movePromo(m) == 6) {
            if(staticEval + 130 < alpha) {
                continue;
            }
        }

        b.makeMove(m);
        Score score;

        if(searched == 0){
            score = -alphaBeta(b, depth-1, ply+1, -beta, -alpha, true);
        } else {
            int reduction = 0;
            if(depth >= 3 && searched >= 3 && !moveIsCapture(m) && !inCheck){
                bool isKiller = (m == killerMoves[ply][0] || m == killerMoves[ply][1]);
                int historyScore = historyTable[b.sideToMove][getMvPiece(m)][moveTo(m)];
                if(!isKiller && historyScore < 10000) {
                    reduction = 1;
                    if(searched >= 6)  reduction = 2;
                    if(searched >= 12) reduction = 3;
                    if(isPV) reduction = std::max(0, reduction-1);
                }
            }
            score = -alphaBeta(b, depth-1-reduction, ply+1, -alpha-1, -alpha, true);
            if(score > alpha && reduction > 0)
                score = -alphaBeta(b, depth-1, ply+1, -alpha-1, -alpha, true);
            if(score > alpha && score < beta)
                score = -alphaBeta(b, depth-1, ply+1, -beta, -alpha, true);
        }

        b.undoMove(m);
        searched++;
        if(stop) return 0;

        if(score > bestScore){
            bestScore = score;
            bestMove  = m;
            if(score > alpha){
                alpha = score;
                flag  = TT_EXACT;
                if(ply == 0){ info.bestMove = m; info.score = score; }
                if(score >= beta){
                    if(!moveIsCapture(m)){
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = m;
                        historyTable[b.sideToMove][getMvPiece(m)][moveTo(m)] += depth*depth;
                    }
                    tt.store(b.hash, depth, score, m, TT_LOWER);
                    info.pruned++;
                    return beta;
                }
            }
        }
    }

    tt.store(b.hash, depth, bestScore, bestMove, flag);
    return bestScore;
}

std::string Search::buildPV(Board& b, int depth){
    if(depth == 0) return "";
    TTEntry* tte = tt.probe(b.hash);
    if(!tte || tte->bestMove == 0) return "";
    Move m = tte->bestMove;
    auto legal = MoveGen::generateLegal(b);
    bool found = false;
    for(Move lm : legal) if(lm==m){ found=true; break; }
    if(!found) return "";
    auto toUCI = [](Move mv)->std::string{
        std::string s;
        s+=(char)('a'+(moveFrom(mv)&7)); s+=(char)('1'+(moveFrom(mv)>>3));
        s+=(char)('a'+(moveTo(mv)&7));   s+=(char)('1'+(moveTo(mv)>>3));
        if(movePromo(mv)!=6){ const char pr[]="nbrq"; s+=pr[movePromo(mv)-KNIGHT]; }
        return s;
    };
    b.makeMove(m);
    std::string rest = buildPV(b, depth-1);
    b.undoMove(m);
    return toUCI(m)+(rest.empty()?"":" "+rest);
}

Move Search::startSearch(Board& board, int maxDepth, int timeLimitMs_){
    timeLimitMs = timeLimitMs_;
    startTime   = std::chrono::steady_clock::now();
    stop        = false;
    info        = {};
    memset(killerMoves,  0, sizeof(killerMoves));
    memset(historyTable, 0, sizeof(historyTable));

    Move  bestMove    = 0;
    Score prevScore   = 0;
    int   aspirWindow = 50;

    for(int depth = 1; depth <= maxDepth; depth++){
        if(stop) break;
        long long used = elapsed_ms(startTime);
        if(used > timeLimitMs * 6 / 10 && depth >= 4) break;

        Score alpha = -INF, beta = INF;
        if(depth >= 4){
            alpha = prevScore - aspirWindow;
            beta  = prevScore + aspirWindow;
        }

        Score score;
        while(true){
            score = alphaBeta(board, depth, 0, alpha, beta, false);
            if(stop) break;
            if     (score <= alpha){ alpha -= aspirWindow; aspirWindow *= 2; }
            else if(score >= beta) { beta  += aspirWindow; aspirWindow *= 2; }
            else break;
            if(alpha < -INF/2) alpha = -INF;
            if(beta  >  INF/2) beta  =  INF;
        }

        if(stop && depth > 1) break;
        if(info.bestMove) bestMove = info.bestMove;

        prevScore   = score;
        aspirWindow = 50;
        info.depth  = depth;
        info.score  = score;
        info.pvLine = buildPV(board, depth);
        info.timeMs = elapsed_ms(startTime);
        info.nodesPerSec = (info.timeMs > 0) ? (info.nodes*1000/info.timeMs) : 0;

        std::cout << "info depth " << depth
                  << " seldepth "  << info.seldepth
                  << " score cp "  << score
                  << " nodes "     << info.nodes
                  << " nps "       << info.nodesPerSec
                  << " time "      << info.timeMs
                  << " pv "        << info.pvLine << "\n";
        std::cout.flush();

        if(score > MATE_SCORE-100 || score < -MATE_SCORE+100) break;
    }

    return bestMove ? bestMove : info.bestMove;
}
