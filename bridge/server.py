"""
ChessMind Bridge — FastAPI server
- Manages the C++ engine process (UCI protocol)
- Provides WebSocket for real-time game communication
- Integrates Stockfish for hints (top-3 moves with scores)
- Computes evaluation bar score
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
import asyncio
import subprocess
import json
import chess
import chess.engine
import os
import re
from typing import Optional

app = FastAPI(title="ChessMind Bridge")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── Engine paths ─────────────────────────────────────────────────────────────
CHESSMIND_PATH = os.environ.get("CHESSMIND_BIN", "./chessmind")
STOCKFISH_PATH = os.environ.get("STOCKFISH_PATH", "stockfish")

# ─── Difficulty → depth + time ────────────────────────────────────────────────
DIFFICULTY = {
    "beginner":     {"depth": 2,  "movetime": 500},
    "intermediate": {"depth": 5,  "movetime": 1000},
    "hard":         {"depth": 8,  "movetime": 2000},
    "expert":       {"depth": 12, "movetime": 4000},
}

# ─── UCI Engine wrapper ───────────────────────────────────────────────────────
class UCIEngine:
    def __init__(self, path: str):
        self.process = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")

    def _send(self, cmd: str):
        self.process.stdin.write(cmd + "\n")
        self.process.stdin.flush()

    def _read_line(self, timeout=10.0) -> str:
        import select
        r, _, _ = select.select([self.process.stdout], [], [], timeout)
        if r:
            return self.process.stdout.readline().strip()
        return ""

    def _wait_for(self, token: str, timeout=10.0) -> list[str]:
        lines = []
        while True:
            line = self._read_line(timeout)
            lines.append(line)
            if token in line:
                break
        return lines

    def new_game(self):
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def set_position(self, fen: str, moves: list[str] = []):
        move_str = " ".join(moves)
        if moves:
            self._send(f"position fen {fen} moves {move_str}")
        else:
            self._send(f"position fen {fen}")

    def get_best_move(self, depth: int, movetime: int) -> dict:
        self._send(f"go depth {depth} movetime {movetime}")
        info = {"depth": 0, "score": 0, "nodes": 0, "nps": 0, "pv": ""}
        best_move = None

        while True:
            line = self._read_line(timeout=movetime/1000 + 5)
            if line.startswith("info"):
                # Parse info line
                dm = re.search(r"depth (\d+)", line)
                sm = re.search(r"score cp (-?\d+)", line)
                nm = re.search(r"nodes (\d+)", line)
                pm = re.search(r"nps (\d+)", line)
                pv = re.search(r"pv (.+)$", line)
                if dm: info["depth"] = int(dm.group(1))
                if sm: info["score"] = int(sm.group(1))
                if nm: info["nodes"] = int(nm.group(1))
                if pm: info["nps"] = int(pm.group(1))
                if pv: info["pv"] = pv.group(1).strip()
            elif line.startswith("bestmove"):
                best_move = line.split()[1]
                break

        return {"move": best_move, **info}

    def get_eval(self) -> int:
        self._send("eval")
        line = self._read_line()
        m = re.search(r"eval (-?\d+)", line)
        return int(m.group(1)) if m else 0

    def close(self):
        self._send("quit")
        self.process.wait()


# ─── Game session ─────────────────────────────────────────────────────────────
class GameSession:
    def __init__(self, session_id: str, difficulty: str = "hard", human_color: str = "white"):
        self.session_id   = session_id
        self.difficulty   = DIFFICULTY.get(difficulty, DIFFICULTY["hard"])
        self.human_color  = chess.WHITE if human_color == "white" else chess.BLACK
        self.board        = chess.Board()
        self.move_history : list[str] = []

        # Launch our C++ engine
        try:
            self.engine = UCIEngine(CHESSMIND_PATH)
            self.engine.new_game()
        except Exception as e:
            print(f"[WARN] Could not launch ChessMind engine: {e}")
            self.engine = None

        # Launch Stockfish for hints
        try:
            self.stockfish = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
        except Exception as e:
            print(f"[WARN] Stockfish not available: {e}")
            self.stockfish = None

    def apply_move(self, uci_move: str) -> bool:
        try:
            move = chess.Move.from_uci(uci_move)
            if move in self.board.legal_moves:
                self.board.push(move)
                self.move_history.append(uci_move)
                return True
        except Exception:
            pass
        return False

    def get_engine_move(self) -> dict:
        if not self.engine:
            return {"error": "Engine not available"}
        self.engine.set_position(self.board.fen(), [])
        result = self.engine.get_best_move(
            self.difficulty["depth"],
            self.difficulty["movetime"]
        )
        return result

    def get_hints(self, top_n: int = 3) -> list[dict]:
        if not self.stockfish:
            return []
        try:
            analysis = self.stockfish.analyse(
                self.board,
                chess.engine.Limit(depth=18),
                multipv=top_n,
            )
            hints = []
            for i, info in enumerate(analysis):
                score = info["score"].pov(self.board.turn)
                cp = score.score(mate_score=10000) or 0
                pv = [m.uci() for m in info.get("pv", [])[:5]]
                if pv:
                    hints.append({
                        "rank": i + 1,
                        "move": pv[0],
                        "score_cp": cp,
                        "score_text": f"+{cp/100:.1f}" if cp >= 0 else f"{cp/100:.1f}",
                        "continuation": pv[1:],
                        "quality": "best" if i == 0 else ("good" if i == 1 else "ok"),
                    })
            return hints
        except Exception as e:
            print(f"Hint error: {e}")
            return []

    def get_eval_bar(self) -> dict:
        """Returns evaluation from White's perspective in centipawns"""
        if not self.stockfish:
            return {"score_cp": 0, "type": "cp"}
        try:
            info = self.stockfish.analyse(self.board, chess.engine.Limit(depth=14))
            score = info["score"].white()
            if score.is_mate():
                m = score.mate()
                return {"score_cp": 10000 if m > 0 else -10000, "type": "mate", "mate_in": m}
            cp = score.score() or 0
            return {"score_cp": max(-1000, min(1000, cp)), "type": "cp"}
        except Exception:
            return {"score_cp": 0, "type": "cp"}

    def get_state(self) -> dict:
        return {
            "fen": self.board.fen(),
            "turn": "white" if self.board.turn == chess.WHITE else "black",
            "is_check": self.board.is_check(),
            "is_checkmate": self.board.is_checkmate(),
            "is_stalemate": self.board.is_stalemate(),
            "is_draw": self.board.is_insufficient_material() or self.board.can_claim_draw(),
            "move_count": self.board.fullmove_number,
            "half_moves": self.board.halfmove_clock,
            "last_move": self.move_history[-1] if self.move_history else None,
        }

    def close(self):
        if self.engine:    self.engine.close()
        if self.stockfish: self.stockfish.quit()


# ─── Active sessions ──────────────────────────────────────────────────────────
sessions: dict[str, GameSession] = {}


# ─── WebSocket endpoint ───────────────────────────────────────────────────────
@app.websocket("/ws/{session_id}")
async def websocket_endpoint(websocket: WebSocket, session_id: str):
    await websocket.accept()
    session: Optional[GameSession] = None

    async def send(data: dict):
        await websocket.send_text(json.dumps(data))

    try:
        while True:
            raw = await websocket.receive_text()
            msg = json.loads(raw)
            action = msg.get("action")

            # ── New game ───────────────────────────────────────────────────────
            if action == "new_game":
                if session: session.close()
                difficulty = msg.get("difficulty", "hard")
                human_color = msg.get("human_color", "white")
                session = GameSession(session_id, difficulty, human_color)
                sessions[session_id] = session
                state = session.get_state()
                eval_bar = await asyncio.get_event_loop().run_in_executor(
                    None, session.get_eval_bar)
                await send({"type": "game_started", "state": state,
                            "eval_bar": eval_bar, "difficulty": difficulty,
                            "human_color": human_color})

                # If AI plays first (human is black)
                if human_color == "black":
                    await _ai_move(session, send)

            # ── Human move ─────────────────────────────────────────────────────
            elif action == "human_move":
                if not session:
                    await send({"type": "error", "msg": "No active game"})
                    continue
                uci = msg.get("move", "")
                ok = session.apply_move(uci)
                if not ok:
                    await send({"type": "error", "msg": "Illegal move"})
                    continue

                state = session.get_state()
                eval_bar = await asyncio.get_event_loop().run_in_executor(
                    None, session.get_eval_bar)
                await send({"type": "move_made", "move": uci,
                            "by": "human", "state": state, "eval_bar": eval_bar})

                if state["is_checkmate"] or state["is_stalemate"] or state["is_draw"]:
                    await send({"type": "game_over", "state": state})
                    continue

                # AI responds
                await _ai_move(session, send)

            # ── Request hints ──────────────────────────────────────────────────
            elif action == "get_hints":
                if not session:
                    await send({"type": "error", "msg": "No active game"}); continue
                hints = await asyncio.get_event_loop().run_in_executor(
                    None, session.get_hints)
                await send({"type": "hints", "hints": hints})

            # ── Request eval bar ───────────────────────────────────────────────
            elif action == "get_eval":
                if not session:
                    await send({"type": "error", "msg": "No active game"}); continue
                eval_bar = await asyncio.get_event_loop().run_in_executor(
                    None, session.get_eval_bar)
                await send({"type": "eval_update", "eval_bar": eval_bar})

            # ── Resign ─────────────────────────────────────────────────────────
            elif action == "resign":
                await send({"type": "game_over",
                            "state": {**session.get_state(), "resigned": True}})

    except WebSocketDisconnect:
        pass
    except Exception as e:
        print(f"WS error: {e}")
    finally:
        if session: session.close()
        sessions.pop(session_id, None)


async def _ai_move(session: GameSession, send):
    """Run AI move in thread pool, stream progress back"""
    await send({"type": "ai_thinking", "thinking": True})
    loop = asyncio.get_event_loop()

    result = await loop.run_in_executor(None, session.get_engine_move)
    ai_uci = result.get("move", "")

    if ai_uci and ai_uci != "0000":
        session.apply_move(ai_uci)
        state = session.get_state()
        eval_bar = await loop.run_in_executor(None, session.get_eval_bar)
        await send({
            "type": "move_made",
            "move": ai_uci,
            "by": "ai",
            "state": state,
            "eval_bar": eval_bar,
            "engine_info": {
                "depth": result.get("depth", 0),
                "score": result.get("score", 0),
                "nodes": result.get("nodes", 0),
                "nps": result.get("nps", 0),
                "pv": result.get("pv", ""),
            }
        })
        if state["is_checkmate"] or state["is_stalemate"] or state["is_draw"]:
            await send({"type": "game_over", "state": state})
    await send({"type": "ai_thinking", "thinking": False})


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")
