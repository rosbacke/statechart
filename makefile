
GTEST_ROOT:=/usr/src/gtest
INC := -I$(GTEST_ROOT)/include/ -Isrc
LIB:= -L$(GTEST_ROOT)

all:
	g++ -std=c++14 $(INC) $(LIB) src/StateChart.cpp test/fsm_test.cpp test/fsm_test2.cpp -l:libgtest.a -pthread
