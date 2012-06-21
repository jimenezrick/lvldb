#!/bin/sh

g++ -g -static -DNO_OUTPUT -std=c++11 -Wall -pedantic -pthread disruptor_test.cpp -o disruptor_test_assert
