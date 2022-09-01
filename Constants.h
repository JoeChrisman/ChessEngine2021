//
// Created by Joe Chrisman on 2/23/22.
//

/*
 * The pieces on the chessboard are represented using bitboards. This means we use a 64 bit integer for each piece type.
 * When the bit is a 1, a piece of that type exists on that square on the board. Each square has a unique index.
 * Square 0 on the board is the top left square. this square is the least significant bit in the bitboard
 * Square 63 on the board is the bottom right square. this square is the most significant bit in the bitboard
 * The engine's pieces are always at the top of the screen. They are closer to the least significant (rightmost) bit of the bitboard.
 * The player's pieces are always at the bottom of the screen. They are closer to the most significant (leftmost) bit of the bitboard.
 *
 * For example, the initial engine pawn bitboard looks like this:
 *
 * lsb
 * 0 0 0 0 0 0 0 0
 * 1 1 1 1 1 1 1 1
 * 0 0 0 0 0 0 0 0
 * 0 0 0 0 0 0 0 0
 * 0 0 0 0 0 0 0 0
 * 0 0 0 0 0 0 0 0
 * 0 0 0 0 0 0 0 0
 * 0 0 0 0 0 0 0 0
 *               msb
 *
 * (0x000000000000FF00 in hex)
 * msb               lsb
 *
 * the square A8 would be index 0. It's bitboard would be 0x0000000000000001.
 * the square H1 would be index 63. It's bitboard would be 0x8000000000000000.
 * the square C7 would be index 10. It's bitboard would be 0x0000000000000200.
 */

#ifndef UNTITLED2_CONSTANTS_H
#define UNTITLED2_CONSTANTS_H

#include <vector>
#include <iostream>
#include <cstdint>
#include <string>
#include <iostream>
#include "SDL2/SDL.h"

const int SQUARE_SIZE = 100;
const int FRAMERATE = 60;
// when an animated piece is moving, it travels along ANIMATION_SPEED line segments.
// the line segments lead from origin square to destination square
const int ANIMATION_SPEED = 15;
const bool ENGINE_IS_WHITE = true;

const int WINDOW_SIZE = SQUARE_SIZE * 8;
const int SEARCH_DEPTH = 5;

#define distance(ax, ay, bx, by) (int)(sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)))
#define isOnBoard(row, col) (row >= 0 && row < 8 && col >= 0 && col < 8)

enum PieceType
{
    PLAYER_PAWN,
    PLAYER_KNIGHT,
    PLAYER_BISHOP,
    PLAYER_ROOK,
    PLAYER_QUEEN,
    PLAYER_KING,

    ENGINE_PAWN,
    ENGINE_KNIGHT,
    ENGINE_BISHOP,
    ENGINE_ROOK,
    ENGINE_QUEEN,
    ENGINE_KING,

    NONE
};

const int PIECE_VALUES[12] = {100, 350, 400, 500, 800, 0, 100, 350, 400, 500, 800, 0};

const PieceType INITIAL_BOARD[64] = {
        ENGINE_ROOK, ENGINE_KNIGHT, ENGINE_BISHOP, ENGINE_IS_WHITE ? ENGINE_KING : ENGINE_QUEEN, ENGINE_IS_WHITE ? ENGINE_QUEEN : ENGINE_KING, ENGINE_BISHOP, ENGINE_KNIGHT, ENGINE_ROOK,
        ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN, ENGINE_PAWN,
        NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
        NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
        NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
        NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
        PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN, PLAYER_PAWN,
        PLAYER_ROOK, PLAYER_KNIGHT, PLAYER_BISHOP, ENGINE_IS_WHITE ? PLAYER_KING : PLAYER_QUEEN, ENGINE_IS_WHITE ? PLAYER_QUEEN : PLAYER_KING, PLAYER_BISHOP, PLAYER_KNIGHT, PLAYER_ROOK,
};

const int MAX_EVAL = 1 << 15;
const int MIN_EVAL = -MAX_EVAL;

const uint64_t RANK0 = 0xFF;
const uint64_t RANK1 = RANK0 << 8;
const uint64_t RANK2 = RANK1 << 8;
const uint64_t RANK3 = RANK2 << 8;
const uint64_t RANK4 = RANK3 << 8;
const uint64_t RANK5 = RANK4 << 8;
const uint64_t RANK6 = RANK5 << 8;
const uint64_t RANK7 = RANK6 << 8;

const uint64_t FILE0 = 0x0101010101010101;
const uint64_t FILE1 = FILE0 << 1;
const uint64_t FILE2 = FILE1 << 1;
const uint64_t FILE3 = FILE2 << 1;
const uint64_t FILE4 = FILE3 << 1;
const uint64_t FILE5 = FILE4 << 1;
const uint64_t FILE6 = FILE5 << 1;
const uint64_t FILE7 = FILE6 << 1;

// pre-initialized bitboards showing what squares need to not be attacked while castling a certain way.
// we cannot castle through check, into check, or out of check.
// so these numbers represent all the squares from the king to the king destination square for a given type of castling.
// the bitboards include both the king's square and the destination square
const uint64_t PLAYER_KINGSIDE_CASTLE = ENGINE_IS_WHITE ? 0xE00000000000000 : 0x7000000000000000;
const uint64_t ENGINE_KINGSIDE_CASTLE = ENGINE_IS_WHITE ? 0x00000000000000E : 0x0000000000000070;
const uint64_t PLAYER_QUEENSIDE_CASTLE = ENGINE_IS_WHITE ? 0x3800000000000000 : 0x1C00000000000000;
const uint64_t ENGINE_QUEENSIDE_CASTLE = ENGINE_IS_WHITE ? 0x0000000000000038 : 0x000000000000001C;

// some extra bitboards so we can isolate the castling destination square
const uint64_t ENGINE_QUEENSIDE_DESTINATION = ENGINE_IS_WHITE ? 0x0000000000000020 : 0x0000000000000004;
const uint64_t ENGINE_KINGSIDE_DESTINATION = ENGINE_IS_WHITE ? 0x0000000000000002 : 0x0000000000000040;
const uint64_t PLAYER_KINGSIDE_DESTINATION = ENGINE_IS_WHITE ? 0x0200000000000000 : 0x4000000000000000;
const uint64_t PLAYER_QUEENSIDE_DESTINATION = ENGINE_IS_WHITE ? 0x2000000000000000 : 0x0400000000000000;

const uint64_t PLAYER_KINGSIDE_ROOK = ENGINE_IS_WHITE ? 0x0100000000000000 : 0x8000000000000000;
const uint64_t ENGINE_KINGSIDE_ROOK = ENGINE_IS_WHITE ? 0x0000000000000001 : 0x0000000000000080;
const uint64_t PLAYER_QUEENSIDE_ROOK =  ENGINE_IS_WHITE ? 0x8000000000000000 : 0x0100000000000000;
const uint64_t ENGINE_QUEENSIDE_ROOK = ENGINE_IS_WHITE ? 0x0000000000000080 : 0x0000000000000001;

const uint64_t CENTER_36_SQUARES = 0x007E7E7E7E7E7E00;
const uint64_t CENTER_16_SQUARES = 0x00003C3C3C3C0000;
const uint64_t CENTER_4_SQUARES = 0x0000001818000000;

const uint64_t OUTER_SQUARES = 0xFF818181818181FF;
const uint64_t FILLED_BOARD = 0xFFFFFFFFFFFFFFFF;

const uint64_t ENGINE_ADVANCED_PAWNS = 0x00003C3C3C000000;
const uint64_t PLAYER_ADVANCED_PAWNS = 0x0000003C3C3C0000;
const uint64_t PAWN_CENTER = 0x0000003C3C000000;

namespace Colors
{
    const int DARK_SQUARE_R = 0;
    const int DARK_SQUARE_G = 0;
    const int DARK_SQUARE_B = 0;

    const int LIGHT_SQUARE_R = 255;
    const int LIGHT_SQUARE_G = 255;
    const int LIGHT_SQUARE_B = 255;

    const int MOVE_OPTION_R = 200;
    const int MOVE_OPTION_G = 0;
    const int MOVE_OPTION_B = 0;

    const int PREVIOUS_MOVE_R = 255;
    const int PREVIOUS_MOVE_G = 255;
    const int PREVIOUS_MOVE_B = 190;

    const int WARNING_R = 255;
    const int WARNING_G = 0;
    const int WARNING_B = 0;

    const int ARROW_R = 70;
    const int ARROW_G = 230;
    const int ARROW_B = 40;
}

// a length 64 array of precalculated bitboards for knight moves.
// for example, index 0 is the bitboard for the pseudo legal knight moves for square 0 (A8) and so on
const uint64_t KNIGHT_MOVES[64] = {
        0x20400,
        0x50800,
        0xa1100,
        0x142200,
        0x284400,
        0x508800,
        0xa01000,
        0x402000,
        0x2040004,
        0x5080008,
        0xa110011,
        0x14220022,
        0x28440044,
        0x50880088,
        0xa0100010,
        0x40200020,
        0x204000402,
        0x508000805,
        0xa1100110a,
        0x1422002214,
        0x2844004428,
        0x5088008850,
        0xa0100010a0,
        0x4020002040,
        0x20400040200,
        0x50800080500,
        0xa1100110a00,
        0x142200221400,
        0x284400442800,
        0x508800885000,
        0xa0100010a000,
        0x402000204000,
        0x2040004020000,
        0x5080008050000,
        0xa1100110a0000,
        0x14220022140000,
        0x28440044280000,
        0x50880088500000,
        0xa0100010a00000,
        0x40200020400000,
        0x204000402000000,
        0x508000805000000,
        0xa1100110a000000,
        0x1422002214000000,
        0x2844004428000000,
        0x5088008850000000,
        0xa0100010a0000000,
        0x4020002040000000,
        0x400040200000000,
        0x800080500000000,
        0x1100110a00000000,
        0x2200221400000000,
        0x4400442800000000,
        0x8800885000000000,
        0x100010a000000000,
        0x2000204000000000,
        0x4020000000000,
        0x8050000000000,
        0x110a0000000000,
        0x22140000000000,
        0x44280000000000,
        0x88500000000000,
        0x10a00000000000,
        0x20400000000000
};

// a length 64 array of precalculated bitboards for knight moves.
// for example, index 63 is the bitboard for the pseudo legal knight moves for square 63 (H1) and so on
const uint64_t KING_MOVES[64] = {
        0x302,
        0x705,
        0xe0a,
        0x1c14,
        0x3828,
        0x7050,
        0xe0a0,
        0xc040,
        0x30203,
        0x70507,
        0xe0a0e,
        0x1c141c,
        0x382838,
        0x705070,
        0xe0a0e0,
        0xc040c0,
        0x3020300,
        0x7050700,
        0xe0a0e00,
        0x1c141c00,
        0x38283800,
        0x70507000,
        0xe0a0e000,
        0xc040c000,
        0x302030000,
        0x705070000,
        0xe0a0e0000,
        0x1c141c0000,
        0x3828380000,
        0x7050700000,
        0xe0a0e00000,
        0xc040c00000,
        0x30203000000,
        0x70507000000,
        0xe0a0e000000,
        0x1c141c000000,
        0x382838000000,
        0x705070000000,
        0xe0a0e0000000,
        0xc040c0000000,
        0x3020300000000,
        0x7050700000000,
        0xe0a0e00000000,
        0x1c141c00000000,
        0x38283800000000,
        0x70507000000000,
        0xe0a0e000000000,
        0xc040c000000000,
        0x302030000000000,
        0x705070000000000,
        0xe0a0e0000000000,
        0x1c141c0000000000,
        0x3828380000000000,
        0x7050700000000000,
        0xe0a0e00000000000,
        0xc040c00000000000,
        0x203000000000000,
        0x507000000000000,
        0xa0e000000000000,
        0x141c000000000000,
        0x2838000000000000,
        0x5070000000000000,
        0xa0e0000000000000,
        0x40c0000000000000
};

#endif //UNTITLED2_CONSTANTS_H
