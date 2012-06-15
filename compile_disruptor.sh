#!/bin/sh

g++ -g -std=c++11 -Wall -pedantic -pthread disruptor_test.cpp disruptor.cpp -o disruptor_test
