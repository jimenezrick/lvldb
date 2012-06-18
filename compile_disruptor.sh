#!/bin/sh

g++ -g -std=c++11 -Wall -Wextra -pedantic -pthread disruptor_test.cpp disruptor.cpp -o disruptor_test
