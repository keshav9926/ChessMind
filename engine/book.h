#pragma once
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <ctime>

// ─── Opening book ─────────────────────────────────────────────────────────────
// Maps FEN position key (piece placement + side) to list of good book moves
// Multiple moves per position = engine picks randomly among them (variety)
// All moves in UCI format (from+to+promo)

class OpeningBook {
public:
    OpeningBook(){
        srand(42); // deterministic for testing; change for variety
        build();
    }

    // Returns a book move UCI string, or "" if not in book
    std::string probe(const std::string& fen) const {
        // Key = first two fields of FEN (position + side to move)
        std::string key = fenKey(fen);
        auto it = book.find(key);
        if(it == book.end()) return "";
        const auto& moves = it->second;
        return moves[rand() % moves.size()];
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> book;

    static std::string fenKey(const std::string& fen){
        // Use first 2 space-separated tokens (board + side)
        size_t p1 = fen.find(' ');
        size_t p2 = fen.find(' ', p1+1);
        return (p2 != std::string::npos) ? fen.substr(0, p2) : fen;
    }

    void add(const std::string& fen, std::initializer_list<std::string> moves){
        book[fenKey(fen)] = std::vector<std::string>(moves);
    }

    void build(){
        // ── Starting position ────────────────────────────────────────────────
        add("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            {"e2e4","d2d4","g1f3","c2c4"});

        // ── After 1.e4 ───────────────────────────────────────────────────────
        add("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
            {"e7e5","c7c5","e7e6","c7c6"});

        // 1.e4 e5
        add("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
            {"g1f3","f2f4","b1c3"});
        // 1.e4 e5 2.Nf3
        add("rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
            {"b8c6","g8f6","d7d6"});
        // 1.e4 e5 2.Nf3 Nc6 — Ruy Lopez / Italian
        add("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
            {"f1b5","f1c4","d2d4","b1c3"});
        // Italian game: 1.e4 e5 2.Nf3 Nc6 3.Bc4
        add("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
            {"f8c5","g8f6","f8e7","d7d6"});
        // Italian 3...Bc5 4.c3
        add("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
            {"c2c3","d2d3","b1c3"});
        // Ruy Lopez: 3.Bb5 a6
        add("r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4",
            {"b5a4","b5c6","e1g1"});

        // 1.e4 c5 — Sicilian
        add("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
            {"g1f3","b1c3","f2f4"});
        // Sicilian 2.Nf3 d6
        add("rnbqkbnr/pp2pppp/3p4/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3",
            {"d2d4","f1b5","b1c3"});
        // Sicilian 2.Nf3 Nc6
        add("r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
            {"d2d4","f1b5","b1c3"});
        // Sicilian 2.Nf3 d6 3.d4 cxd4 4.Nxd4 Nf6 5.Nc3 — Najdorf/Dragon setup
        add("rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R b KQkq - 1 5",
            {"a7a6","g7g6","e7e6","b8c6"});

        // 1.e4 e6 — French
        add("rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
            {"d2d4","b1c3","g1f3"});
        // French 2.d4 d5 3.Nc3
        add("rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3",
            {"g8f6","f8b4","d5e4","c7c5"});

        // 1.e4 c6 — Caro-Kann
        add("rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
            {"d2d4","b1c3","g1f3"});

        // ── After 1.d4 ───────────────────────────────────────────────────────
        add("rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1",
            {"d7d5","g8f6","e7e6","f7f5"});
        // QGD: 1.d4 d5 2.c4
        add("rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
            {"e7e6","c7c6","d5c4","g8f6"});
        // 1.d4 Nf6 2.c4
        add("rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
            {"e7e6","g7g6","c7c5","d7d5"});
        // King's Indian: 1.d4 Nf6 2.c4 g6 3.Nc3 Bg7 4.e4 d6
        add("rnbqk2r/ppp1ppbp/3p1np1/8/2PPP3/2N5/PP3PPP/R1BQKBNR w KQkq - 1 5",
            {"g1f3","f2f3","f1e2"});

        // ── After 1.Nf3 ──────────────────────────────────────────────────────
        add("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1",
            {"d7d5","g8f6","c7c5","e7e6"});

        // ── After 1.c4 — English ─────────────────────────────────────────────
        add("rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1",
            {"e7e5","g8f6","c7c5","e7e6"});

        // ── Castled positions: general good moves ────────────────────────────
        // White castled kingside, standard pawn center
        add("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQkq - 0 1",
            {"e1g1"});
        add("r1bq1rk1/pppp1ppp/2n2n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 1",
            {"c1g5","a2a3","d3d4","h2h3"});
    }
};
