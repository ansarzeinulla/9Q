CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O3 -march=native

TARGET = find_shortest_game

all: $(TARGET)

$(TARGET): find_shortest_game.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) find_shortest_game.cpp

clean:
	rm -f $(TARGET)

.PHONY: all clean
