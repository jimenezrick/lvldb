#!/bin/sh

g++ -static -DNDEBUG -DNO_OUTPUT -O3 -std=c++11 -Wall -pedantic -pthread disruptor_test.cpp disruptor.cpp -o disruptor_test_static
