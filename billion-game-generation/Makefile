CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O3 -march=native -pthread

ENGINE = ../engine/togyzkumalak_rules.cpp ../engine/position_hash.cpp

TARGET = generate_billion_games
SRCS = generate_billion_games.cpp $(ENGINE)

all: $(TARGET) 

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
