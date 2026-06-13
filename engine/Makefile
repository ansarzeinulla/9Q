CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -pthread

SRCS = togyzkumalak_rules.cpp position_hash.cpp minimax_engine.cpp dag_search.cpp evaluation.cpp togyzkumalak_engine.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = togyzkumalak_engine
RULE_TEST = ../tests/rules_test
RULE_TEST_SRCS = ../tests/rules_test.cpp togyzkumalak_rules.cpp position_hash.cpp minimax_engine.cpp evaluation.cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RULE_TEST): $(RULE_TEST_SRCS) togyzkumalak_rules.hpp position_hash.hpp
	$(CXX) $(CXXFLAGS) -I. $(RULE_TEST_SRCS) -o $(RULE_TEST)

test: $(RULE_TEST)
	$(RULE_TEST)

clean:
	rm -f $(OBJS) $(TARGET) $(RULE_TEST)

.PHONY: all clean test
