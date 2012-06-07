#!/bin/sh

g++ -std=c++11 -Wall -Wextra -Wno-attributes -pedantic bloom.cpp murmurhash/MurmurHash3.cpp -o bloom
