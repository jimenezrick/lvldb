#!/bin/sh

g++ -g -std=c++11 -Wall -Wno-attributes -pedantic bloom_test.cpp bloom.cpp murmurhash/MurmurHash3.cpp -o bloom_test
