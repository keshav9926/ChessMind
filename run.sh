#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE_DIR="$SCRIPT_DIR/bridge"
FRONTEND="$SCRIPT_DIR/frontend/index.html"
PORT=8000

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

echo -e "${BOLD}ChessMind${NC} — starting up..."

# Check engine binary exists
if [ ! -f "$BRIDGE_DIR/chessmind" ]; then
    echo -e "${RED}Engine not found. Run ./setup.sh first.${NC}"
    exit 1
fi

# Check Python packages
python3 -c "import fastapi, uvicorn, chess" 2>/dev/null || {
    echo -e "${RED}Python packages missing. Run ./setup.sh first.${NC}"
    exit 1
}

# Kill any existing process on port 8000
if lsof -Pi ":$PORT" -sTCP:LISTEN -t &>/dev/null 2>&1; then
    echo -e "${CYAN}→${NC} Port $PORT in use, stopping old process..."
    lsof -ti ":$PORT" | xargs kill -9 2>/dev/null || true
    sleep 1
fi

# Load env if present
[ -f "$BRIDGE_DIR/.env" ] && export $(cat "$BRIDGE_DIR/.env" | xargs) 2>/dev/null

# Start bridge in background
echo -e "${CYAN}→${NC} Starting bridge on http://localhost:$PORT ..."
cd "$BRIDGE_DIR"
uvicorn server:app --host 0.0.0.0 --port $PORT --log-level warning &
BRIDGE_PID=$!

# Wait for bridge to be ready
sleep 2
if kill -0 $BRIDGE_PID 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Bridge running (PID $BRIDGE_PID)"
else
    echo -e "${RED}Bridge failed to start. Check bridge/server.py${NC}"
    exit 1
fi

# Open browser
echo -e "${CYAN}→${NC} Opening game in browser..."
if command -v xdg-open &>/dev/null; then
    xdg-open "$FRONTEND" 2>/dev/null &
elif command -v open &>/dev/null; then
    open "$FRONTEND" 2>/dev/null &
else
    echo -e "${CYAN}→${NC} Open manually: $FRONTEND"
fi

echo ""
echo -e "${GREEN}${BOLD}Game ready!${NC} Press Ctrl+C to stop."
echo ""

# Keep running, show bridge logs
trap "echo ''; echo 'Stopping...'; kill $BRIDGE_PID 2>/dev/null; exit 0" INT TERM

# Restart bridge if it crashes
while true; do
    if ! kill -0 $BRIDGE_PID 2>/dev/null; then
        echo "Bridge crashed, restarting..."
        cd "$BRIDGE_DIR"
        uvicorn server:app --host 0.0.0.0 --port $PORT --log-level warning &
        BRIDGE_PID=$!
    fi
    sleep 3
done
