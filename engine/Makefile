CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -pthread

SRCS = togyzkumalak_rules.cpp position_hash.cpp minimax_engine.cpp dag_search.cpp evaluation.cpp togyzkumalak_engine.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = togyzkumalak_engine

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
