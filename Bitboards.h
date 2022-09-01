//
// Created by Joe Chrisman on 4/17/22.
//

#ifndef UNTITLED2_BITBOARDS_H
#define UNTITLED2_BITBOARDS_H

/*
 * this is where functions involving bitboards go
 * the top few functions are extremely performance sensitive, because these
 * functions are what we use do basic functions with bitboards
 */

#include "Constants.h"

// pop the least significant set bit off of the bitboard and return the bit index
inline uint8_t popLeastSquare(uint64_t &board)
{
    uint8_t leastSquare = _mm_tzcnt_64(board);
    board ^= ((uint64_t)(1) << leastSquare);
    return leastSquare;
}

// pop the least significant set bit off of the bitboard and return a bitboard with only that bit set
inline uint64_t popLeastBitboard(uint64_t &board)
{
    uint64_t leastBoard = board & (~board + 1);
    board ^= leastBoard;
    return leastBoard;
}

// get the number of ones in the bitboard
inline int countSetBits(uint64_t board)
{
    return __builtin_popcountll(board);
}

// get the index of the least significant set bit
inline uint8_t getLeastSquare(uint64_t board)
{
   return _mm_tzcnt_64(board);
}

// get a bitboard with one set bit, by bit index
inline uint64_t boardOf(uint8_t square)
{
    return (uint64_t)(1) << square;
}

// generate a random 64 bit number by iterating over each bit
// and setting it to be either 0 or 1. kind of slow, but used
// only for generating sliding attack magic numbers on startup
inline uint64_t random64()
{
    uint64_t generated = 0;
    for (int index = 0; index < 64; index++)
    {
        if (rand() % 2 == 0)
        {
            generated |= (uint64_t)(1) << index;
        }
    }
    return generated;
}

// display a bitboard for debugging purposes
inline void print64(uint64_t board)
{
    std::cout << std::string(8, '_');
    for (int i = 0; i < 64; i++)
    {
        if (i % 8 == 0)
        {
            std::cout << std::endl;
        }
        std::cout << ((board % 2) ? 'X' : '.') << " ";

        board >>= 1;
    }
    std::cout << std::endl << std::string(8, '_') << std::endl;
}

#endif //UNTITLED2_BITBOARDS_H
