CXX      = g++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall
SRCS     = board.cpp movegen.cpp eval.cpp search.cpp
TARGET   = chessmind
PERFT    = perft

.PHONY: all clean run perft install

all: $(TARGET)

$(TARGET): main.cpp $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Built $(TARGET)"

perft: perft.cpp $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@./$(PERFT)

install: $(TARGET)
	cp $(TARGET) ../bridge/$(TARGET)
	@echo "Installed to bridge/"

clean:
	rm -f $(TARGET) $(PERFT)
