//
// Created by Joe Chrisman on 5/10/22.
//

#include "Evaluation.h"

/*
 * return a positive value when the engine is winning, and a negative value when the engine is losing
 */
int Evaluation::evaluate(Board::Position &position)
{
    int score = 0;

    // first, calculate material score for both sides
    for (int pieceType = PLAYER_PAWN; pieceType <= PLAYER_QUEEN; pieceType++)
    {
        score -= countSetBits(position.pieces[pieceType]) * PIECE_VALUES[pieceType];
    }

    for (int pieceType = ENGINE_PAWN; pieceType <= ENGINE_QUEEN; pieceType++)
    {
        score += countSetBits(position.pieces[pieceType]) * PIECE_VALUES[pieceType];
    }

    // the player's knights being in the center of the board is bad for the engine.
    score -= countSetBits(position.pieces[PLAYER_KNIGHT] & CENTER_16_SQUARES) * 70;
    // the engine's knights being in the center of the board is good for the engine.
    score += countSetBits(position.pieces[ENGINE_KNIGHT] & CENTER_16_SQUARES) * 70;

    // the player's bishops being in the center of the board is bad for the engine.
    score -= countSetBits(position.pieces[PLAYER_BISHOP] & CENTER_16_SQUARES) * 60;
    // the engine's bishops being in the center of the board is good for the engine.
    score += countSetBits(position.pieces[ENGINE_BISHOP] & CENTER_16_SQUARES) * 60;


    // the player's pawns being in the center is bad for the engine
    score -= countSetBits(position.pieces[PLAYER_PAWN] & PAWN_CENTER) * 10;
    // the player's pawns being in the center four squares is bad for the engine
    score -= countSetBits(position.pieces[PLAYER_PAWN] & CENTER_4_SQUARES) * 30;
    // the player's pawn being advanced in the center is bad for the engine
    score -= countSetBits(position.pieces[PLAYER_PAWN] & PLAYER_ADVANCED_PAWNS) * 15;

    // the engine's pawns being in the center is good for the engine
    score += countSetBits(position.pieces[ENGINE_PAWN] & PAWN_CENTER) * 10;
    // the engine's pawns being in the center four squares is good for the engine
    score += countSetBits(position.pieces[ENGINE_PAWN] & CENTER_4_SQUARES) * 30;
    // the player's pawn being advanced in the center is bad for the engine
    score += countSetBits(position.pieces[ENGINE_PAWN] & ENGINE_ADVANCED_PAWNS) * 15;

    return score;
}
