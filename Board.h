//
// Created by Joe Chrisman on 2/23/22.
//

#ifndef UNTITLED2_BOARD_H
#define UNTITLED2_BOARD_H

#include "Bitboards.h"

class Board
{

public:
    Board();
    bool engineToMove;

    struct Position
    {
        /*
         * the pieces on the board. each element of this array is a 64 bit bitboard.
         * there is one bitboard for each unique piece type
         * this array can be indexed with a PieceType enum to retrieve a bitboard by piece type
         * indexing it with PieceType NONE is undefined behavior
         */
        uint64_t pieces[12];

        // castling rights
        bool playerCastleQueenside;
        bool playerCastleKingside;
        bool engineCastleQueenside;
        bool engineCastleKingside;

        uint64_t enPassantCapture;
    } position;

    // special moves
    enum MoveType
    {
        QUEEN_PROMOTION,
        KNIGHT_PROMOTION,
        BISHOP_PROMOTION,
        ROOK_PROMOTION,
        EN_PASSANT,
        NORMAL
    };

    struct Move
    {
        MoveType type;

        uint8_t from;
        uint8_t to;
        PieceType moving;
        PieceType captured;
    };

    // these are updated when update() is called.
    // they mainly help out move generation
    uint64_t enginePieces;
    uint64_t playerOrEmpty; // movable squares for engine pieces
    uint64_t playerPieces;
    uint64_t engineOrEmpty; // movable squares for player pieces
    uint64_t emptySquares;
    uint64_t occupiedSquares;

    template <bool isEngine>
    inline void makeMove(Move &move)
    {
        uint64_t enPassant = position.enPassantCapture;
        position.enPassantCapture = 0;

        uint64_t &moving = position.pieces[move.moving];

        uint64_t squareTo = boardOf(move.to);
        uint64_t squareFrom = boardOf(move.from);

        // remove the piece from its original square
        moving ^= squareFrom;

        // if we are promoting
        if (move.type < 4)
        {
            // add the promoted piece to the destination square
            if (move.type == QUEEN_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_QUEEN : PLAYER_QUEEN] |= squareTo;
            }
            else if (move.type == ROOK_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] |= squareTo;
            }
            else if (move.type == BISHOP_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_BISHOP : PLAYER_BISHOP] |= squareTo;
            }
            else if (move.type == KNIGHT_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_KNIGHT : PLAYER_KNIGHT] |= squareTo;
            }
        }
        // if we did not promote
        else
        {
            // place the piece onto its new square.
            moving |= squareTo;
        }

        // if we want to capture a piece
        if (move.captured != NONE)
        {
            // remove the captured piece
            position.pieces[move.captured] ^= (move.type == EN_PASSANT ? enPassant : squareTo);

            // if we are capturing a rook
            if (move.captured == (isEngine ? PLAYER_ROOK : ENGINE_ROOK))
            {
                // if our opponent has the right to castle kingside
                if (isEngine ? position.playerCastleKingside : position.engineCastleKingside)
                {
                    // if capturing this rook should forbid kingside castling for our opponent
                    if (squareTo & (isEngine ? PLAYER_KINGSIDE_ROOK : ENGINE_KINGSIDE_ROOK))
                    {
                        // forbid our opponent from kingside castling
                        (isEngine ? position.playerCastleKingside : position.engineCastleKingside) = false;
                    }
                }
                // if our opponent has the right to castle queenside
                if (isEngine ? position.playerCastleQueenside : position.engineCastleQueenside)
                {

                    // if capturing this rook should forbid queenside castling for our opponent
                    if (squareTo & (isEngine ? PLAYER_QUEENSIDE_ROOK : ENGINE_QUEENSIDE_ROOK))
                    {
                        // forbid our opponent from queenside castling
                        (isEngine ? position.playerCastleQueenside : position.engineCastleQueenside) = false;
                    }
                }
            }
        }
        // if we moved the king
        if (move.moving == (isEngine ? ENGINE_KING : PLAYER_KING))
        {
            // if we have the right to castle in any way
            if (isEngine ? position.engineCastleKingside || position.engineCastleQueenside : position.playerCastleKingside || position.playerCastleQueenside)
            {
                // if we are castling kingside
                if (squareTo & (isEngine ? ENGINE_KINGSIDE_DESTINATION : PLAYER_KINGSIDE_DESTINATION))
                {
                    // remove the kingside rook from its square
                    position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] ^= isEngine ? ENGINE_KINGSIDE_ROOK : PLAYER_KINGSIDE_ROOK;
                    // place it down next to the castled king
                    position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] |= (ENGINE_IS_WHITE ? squareTo << 1 : squareTo >> 1);
                }

                // if we are castling queenside
                if (squareTo & (isEngine ? ENGINE_QUEENSIDE_DESTINATION : PLAYER_QUEENSIDE_DESTINATION))
                {
                    // remove the queenside rook from its square
                    position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] ^= isEngine ? ENGINE_QUEENSIDE_ROOK : PLAYER_QUEENSIDE_ROOK;
                    // place it down next to the castled king
                    position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] |= (ENGINE_IS_WHITE ? squareTo >> 1 : squareTo << 1);
                }
                // we moved our king. so remember we can not castle in any direction anymore
                (isEngine ? position.engineCastleKingside : position.playerCastleKingside) = false;
                (isEngine ? position.engineCastleQueenside : position.playerCastleQueenside) = false;

            }
        }
        // if we are moving a rook
        else if (move.moving == (isEngine ? ENGINE_ROOK : PLAYER_ROOK))
        {
            // if moving this rook should forever forbid kingside castling
            if (isEngine ? position.engineCastleKingside : position.playerCastleKingside)
            {
                if (squareFrom & (isEngine ? ENGINE_KINGSIDE_ROOK : PLAYER_KINGSIDE_ROOK))
                {
                    // forbid kingside castling
                    (isEngine ? position.engineCastleKingside : position.playerCastleKingside) = false;
                }
            }
            // if moving this rook should forever forbid queenside castling
            if (isEngine ? position.engineCastleQueenside : position.playerCastleQueenside)
            {
                if (squareFrom & (isEngine ? ENGINE_QUEENSIDE_ROOK : PLAYER_QUEENSIDE_ROOK))
                {
                    // forbid queenside castling
                    (isEngine ? position.engineCastleQueenside : position.playerCastleQueenside) = false;
                }
            }
        }
        // if we are moving a pawn, check if it will be able to be captured en passant
        else if (move.moving == (isEngine ? ENGINE_PAWN : PLAYER_PAWN))
        {
            // if this pawn move is a double push move
            if (abs(move.to - move.from) == 16)
            {
                // if there are enemy pawns to the right or left
                if ((squareTo << 1 | squareTo >> 1) & (isEngine ? RANK3 : RANK4) & position.pieces[isEngine ? PLAYER_PAWN : ENGINE_PAWN])
                {
                    // this pawn can be captured en-passant
                    position.enPassantCapture = squareTo;
                }
            }
        }
        // update some extra bitboards
        update();
        engineToMove = !engineToMove;
    }

    /*
     * this might be bugged. make sure it gives the correct perft results before use
     *
     * for now, it is just sitting here, maybe it will become used one day.
     * until then, we are copying the board state before a move and restoring it after
     *
    template<bool isEngine>
    inline void unmakeMove(Board::Move &move)
    {
        uint64_t squareTo = boardOf(move.to);
        uint64_t squareFrom = boardOf(move.from);

        // put our piece back to where it came from
        position.pieces[move.moving] |= squareFrom;
        position.pieces[move.moving] ^= squareTo;

        // put a captured piece back if we captured one
        if (move.captured != NONE)
        {
            // if we want to undo an en-passant capture
            if (move.type == EN_PASSANT)
            {
                position.pieces[move.captured] |= isEngine ? squareTo >> 8 : squareTo << 8;
            }
            // otherwise, undo a normal capture (this includes promotion capture)
            else
            {
                position.pieces[move.captured] |= squareTo;
            }
        }

        // if we need to un-promote
        if (move.type < 4)
        {
            // remove the promoted piece from its square
            if (move.type == QUEEN_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_QUEEN : PLAYER_QUEEN] ^= squareTo;
            }
            else if (move.type == ROOK_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] ^= squareTo;
            }
            else if (move.type == BISHOP_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_BISHOP : PLAYER_BISHOP] ^= squareTo;
            }
            else if (move.type == KNIGHT_PROMOTION)
            {
                position.pieces[isEngine ? ENGINE_KNIGHT : PLAYER_KNIGHT] ^= squareTo;
            }
        }

        // if we need to un-castle a rook (maybe find a cheap way without % later)
        if (move.moving == (isEngine ? ENGINE_KING : PLAYER_KING) && abs(move.from % 8 - move.to % 8) > 1)
        {
            // if we are un-castling kingside
            if (squareTo == (isEngine ? ENGINE_KINGSIDE_DESTINATION : PLAYER_KINGSIDE_DESTINATION))
            {
                // remove the rook from next to the king
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] ^= ENGINE_IS_WHITE ? squareTo << 1 : squareTo >> 1;
                // put the rook back where it came from
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] |= isEngine ? ENGINE_KINGSIDE_ROOK : PLAYER_QUEENSIDE_ROOK;
            }
            // if we are un-castling queenside
            else if (squareTo == (isEngine ? ENGINE_QUEENSIDE_DESTINATION : PLAYER_QUEENSIDE_DESTINATION))
            {
                // remove the rook from next to the king
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] ^= ENGINE_IS_WHITE ? squareTo >> 1 : squareTo << 1;
                // put the rook back where it came from
                position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK] |= isEngine ? ENGINE_QUEENSIDE_ROOK : PLAYER_QUEENSIDE_ROOK;
            }
        }

        engineToMove = !engineToMove;
    }*/

    inline void update()
    {
        enginePieces = position.pieces[ENGINE_PAWN] | position.pieces[ENGINE_KNIGHT] |
                       position.pieces[ENGINE_BISHOP] | position.pieces[ENGINE_ROOK] |
                       position.pieces[ENGINE_QUEEN] | position.pieces[ENGINE_KING];

        playerPieces = position.pieces[PLAYER_PAWN] | position.pieces[PLAYER_KNIGHT] |
                       position.pieces[PLAYER_BISHOP] | position.pieces[PLAYER_ROOK] |
                       position.pieces[PLAYER_QUEEN] | position.pieces[PLAYER_KING];

        occupiedSquares = enginePieces | playerPieces;
        emptySquares = ~occupiedSquares;
        playerOrEmpty = playerPieces | emptySquares;
        engineOrEmpty = enginePieces | emptySquares;
    }
    /*
     * get an engine piece PieceType enum for any given square
     * if there is no player piece on the square, return NONE
     */
    inline PieceType getPlayerPieceType(uint8_t square)
    {
        uint64_t mask = boardOf(square);
        if (emptySquares & mask)
        {
            return NONE;
        }
        for (int type = PLAYER_PAWN; type < ENGINE_PAWN; type++)
        {
            if (position.pieces[type] & mask)
            {
                return (PieceType)type;
            }
        }
        assert(false);
    }

    /*
     * get an engine piece PieceType enum for any given square
     * if there is no engine piece on the square, return NONE
     */
    inline PieceType getEnginePieceType(uint8_t square)
    {
        uint64_t mask = boardOf(square);
        if (emptySquares & mask)
        {
            return NONE;
        }
        for (int type = ENGINE_PAWN; type < NONE; type++)
        {
            if (position.pieces[type] & mask)
            {
                return (PieceType)type;
            }
        }
        assert(false);
    }

    /*
     * get the PieceType enum for any given square
     * if there is no piece on the square, NONE is returned
     */
    inline PieceType getPieceType(uint8_t square)
    {
        uint64_t mask = boardOf(square);
        if (emptySquares & mask)
        {
            return NONE;
        }
        if (playerPieces & mask)
        {
            return getPlayerPieceType(square);
        }
        if (enginePieces & mask)
        {
            return getEnginePieceType(square);
        }
        assert(false);
    }

    std::string getMoveNotation(Move &move);

};


#endif //UNTITLED2_BOARD_H
