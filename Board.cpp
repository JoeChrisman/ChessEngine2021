//
// Created by Joe Chrisman on 2/23/22.
//

#include "Board.h"

Board::Board()
{
    position = Position{
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            true,
            true,
            true,
            true
    };

    // populate the position bitboards with the initial values
    for (uint8_t square = 0; square < 64; square++)
    {
        uint64_t squareMask = boardOf(square);
        PieceType piece = INITIAL_BOARD[square];
        if (piece != NONE)
        {
            position.pieces[piece] |= squareMask;
        }
    }
    update();
    engineToMove = ENGINE_IS_WHITE;
}

std::string Board::getMoveNotation(Move &move)
{
    assert(move.moving != NONE);

    std::string notation;

    if (move.moving == ENGINE_KNIGHT || move.moving == PLAYER_KNIGHT)
    {
        notation += "n";
    }
    else if (move.moving == ENGINE_BISHOP || move.moving == PLAYER_BISHOP)
    {
        notation += "b";
    }
    else if (move.moving == ENGINE_ROOK || move.moving == PLAYER_ROOK)
    {
        notation += "r";
    }
    else if (move.moving == ENGINE_QUEEN || move.moving == PLAYER_QUEEN)
    {
        notation += "q";
    }
    else if (move.moving == ENGINE_KING || move.moving == PLAYER_KING)
    {
        if (abs(move.from % 8 - move.to % 8) > 1)
        {
            notation += "castle";
        }
        else
        {
            notation += "k";
        }
    }

    if (move.captured != NONE)
    {
        notation += "x";
    }

    if (notation.size() < 2)
    {
        int row = move.to / 8;
        int col = move.to % 8;

        notation += ENGINE_IS_WHITE ? (char)('h' - col) : (char)('a' + col);
        notation += std::to_string(ENGINE_IS_WHITE ? row + 1 : 8 - row);
    }
    return notation;
}

