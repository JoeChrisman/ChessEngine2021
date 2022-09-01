//
// Created by Joe Chrisman on 2/24/22.
//

#ifndef UNTITLED2_MOVEGEN_H
#define UNTITLED2_MOVEGEN_H

#include "Evaluation.h"

class MoveGen
{
public:
    MoveGen(Board *board);

    Board *board;

    bool isKingInCheck(bool isEngine);
    void generateEngineMoves();
    void generatePlayerMoves();
    std::vector<Board::Move> getSortedMoves();

private:
    Board::Position *position;

    /*
     * a bitboard showing the squares we can move a piece to, while considering the opponent's checks
     * if our king is not in check at all, every square is "legal".
     * if our king is in check, then this bitboard is the squares a piece can move to in order to block the check
     * if there are more than 1 checkers, this bitboard is 0, because there is no way to block
     * a double check. the king must step out of the way of the attackers, otherwise it is checkmate.
     */
    uint64_t blockerSquares;

    /*
     * these bitboards represent the squares along the active pin rays in the position.
     * there is one per pin ray type.
     *
     * we need to keep cardinal and ordinal pins seperated to avoid illegally moving a piece out
     * of an absolute pin to block a different absolute pin produced by the opposite sliding movement type
     */
    uint64_t cardinalPins;
    uint64_t ordinalPins;

    /*
     * a vector that gets populated with Move structs that are legal chess moves
     */
    std::vector<Board::Move> generated;

    template<bool isEngine>
    inline void generatePawnMoves();

    template<bool isEngine>
    inline void generateKnightMoves();

    template<bool isEngine>
    inline void generateKingMoves();

    template<bool isEngine>
    inline void generateRookMoves();

    template<bool isEngine>
    inline void generateBishopMoves();

    template<bool isEngine>
    inline void generateQueenMoves();

    /*
     * update the squares that show where a checking piece must be captured or blocked.
     * if there are no checking pieces, every square is a 1. if it is double check, every square is 0
     */
    template<bool isEngine>
    inline void getBlockerSquares();

    template<bool isEngine>
    inline void getOrdinalPins();

    template<bool isEngine>
    inline void getCardinalPins();

    /*
    * a function that tells us whether a square is attacked by the opponent.
    * this function is a little instruction heavy, because it has to check for every
    * possible attacker type and get all the movements again. luckily, we only have to
    * use this function to make sure the king doesn't walk into or castle over check
    */
    template<bool isEngine>
    inline bool isSafeSquare(uint8_t square);

    /*
     * a struct used in my fixed shift plain magic bitboard implementation. a magic square
     * has information useful for hashing the collision occupancy for a sliding piece
     * read more about the magic bitboard technique (plain implementation):
     * https://www.chessprogramming.org/Magic_Bitboards
     *
     * I guess my implementation is not truly "fixed shift" - because rook hash keys
     * are 12 bits long and bishop hash keys are 9 bits to save lookup table space
     */
    struct MagicSquare
    {
        /*
        * a bitboard showing what squares a piece on this square could be blocked by.
        * the squares at the end of the attack ray are truncated - because they cannot block the sliding piece.
        * the less blocking squares there are, there are an order of magnitude less blocker permutations,
        * which is why leaving out the ending squares is a good optimization
        */
        uint64_t blockers;

        // a "magic" number used in the hashing algorithm. this number allows us to map a blocker bitboard to an attack bitboard
        // in the attack table arrays below. it lets us get sliding piece moves in very few instructions
        uint64_t magic;
    };

    MagicSquare ordinals[64]; // bishop magic numbers and blocker bitboards
    MagicSquare cardinals[64]; // rook magic numbers and blocker bitboards

    // 64 squares, each with 4,096 or less different ways of having its cardinal path blocked
    uint64_t cardinalAttacks[64][4096];
    // 64 squares, each with 512 or less different ways of having its ordinal path blocked
    uint64_t ordinalAttacks[64][512];

    /*
     * these functions are slow. they are only used at program startup to populate precalculated
     * attack tables for sliding pieces. sliding pieces are complicated because their movement depends on the
     * placement of other pieces in its path. we take the pieces in the path, do a hash function
     * to them, and use that hash key to index an array of precalculated moves
     *
     * we need to be able to get a bitboard of squares that can block a sliding piece type on a given square.
     * the getRookBlockers()/getBishopBlockers() functions do just that - but they disregard the ending square in the attack ray.
     * it is not included because it is redundant - the ending square in the ray can never block us because there is no
     * square after it in the ray to stop us. making this optimization is crutial because we effectively
     * divide the size of our attack tables by 2 * however many squares we removed in the worst case scenario.
     * without this optimization, the cardinal/ordinal attack tables would both have to be 2^14 in length per square instead of 2^12
     * per square for rooks and 2^9 per square for bishops. making the attack tables any bigger makes it too hard to find a magic number
     */
    uint64_t getRookBlockers(uint8_t from); // squares that could block a rook.
    uint64_t getBishopBlockers(uint8_t from); // squares that could block a bishop.
    uint64_t getRookAttacks(uint8_t from, uint64_t blockers, bool captures); // squares a rook could move to.
    uint64_t getBishopAttacks(uint8_t from, uint64_t blockers, bool captures); // squares a bishop could move to.

    uint64_t getMagicNumber(uint8_t square, bool isCardinal);

};


#endif //UNTITLED2_MOVEGEN_H
