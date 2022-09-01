//
// Created by Joe Chrisman on 2/24/22.
//

#include "MoveGen.h"

/*
 * a lot of magic bitboard setup is happening here, and some algorithms for move generation are quite slow
 * but this part of the code is not performance sensitive because it is only executed once at program start
 */
MoveGen::MoveGen(Board *board)
{
    this->board = board;
    this->position = &board->position;

    // squares we can move a piece to in order to block or capture a checking piece
    blockerSquares = FILLED_BOARD;
    // squares along rank/file pins
    cardinalPins = 0;
    // squares along diagonal pins
    ordinalPins = 0;

    // go through each "magic square" on the board - spending a lot of time giving each square a calculated number
    // that perfectly hashes any blocker bitboard into it's corresponding attack bitboard. these magic numbers can
    // be used again later to index a hash table to figure out the movement of sliding pieces very efficiently.
    for (uint8_t square = 0; square < 64; square++)
    {
        cardinals[square].blockers = getRookBlockers(square);
        ordinals[square].blockers = getBishopBlockers(square);

        cardinals[square].magic = getMagicNumber(square, true);
        ordinals[square].magic = getMagicNumber(square, false);
    }
}

/*
 * the goal of this function is to find a number for a given square and movement type.
 * this "magic" number will be able to be multiplied by a bitboard representing pieces blocking
 * a sliding piece. this product can then be shifted to form a small hash. this hash is then used
 * to index a pre initialized table of attacks. this allows us to get sliding piece moves in very few instructions
 */
uint64_t MoveGen::getMagicNumber(uint8_t square, bool isCardinal)
{
    // a bitboard representing squares that could block the sliding piece
    uint64_t blockerMask = isCardinal ? cardinals[square].blockers : ordinals[square].blockers;
    // number of possible ways the sliding piece can be blocked
    int numPermutations = 1 << countSetBits(blockerMask);

    /*
     * a rook on the corner has 12 squares that can block it. that is the worst case for rooks.
     * therefore, cardinal pieces have a max blocker permutation count of 2^12 (4096).
     * a bishop in one of the center four squares has 9 squares that can block it. that is the worst case for bishops.
     * therefore, ordinal pieces have a max blocker permutation count of 2^9 (512).
     */
    int maxPermutations = isCardinal ? 4096 : 512;

    uint64_t attacks[maxPermutations]; // attack bitboards by blocker permutation index
    uint64_t blockers[maxPermutations]; // blocker bitboard by blocker permutation index
    uint64_t attacksSeen[maxPermutations]; // attack bitboards by "magic" hash index

    // iterate through each possible permutation of blocking pieces
    for (int permutation = 0; permutation < numPermutations; permutation++)
    {
        // the bitboard representing this blocker permutation
        uint64_t actualBlockers = 0;
        int blockerPermutation = permutation;
        uint64_t possibleBlockers = blockerMask;
        // while there are more blocking pieces in this permutation
        while (possibleBlockers)
        {
            // get the blocking piece in bitboard form
            uint64_t blocker = popLeastBitboard(possibleBlockers);
            // only map it to the blocker bitboard if we want to allow the blocker in this permutation
            if (blockerPermutation % 2) {
                // map it to the blocker bitboard
                 actualBlockers |= blocker;
            }
            // visit the next permutation
            blockerPermutation >>= 1;
        }

        // initialize an array of blocker bitboards, indexed by the blocker permutation
        blockers[permutation] = actualBlockers;
        // initialize an array of sliding attack bitboards, indexed by the blocker permutation
        // we can figure out what the attack bitboard should look like using the blocker set
        attacks[permutation] = isCardinal ? getRookAttacks(square, actualBlockers, true)
                                          : getBishopAttacks(square, actualBlockers, true);
    }

    /*
     * try to look for a magic number that works. try 1,000,000 times before giving up.
     * in most cases, this will take far less than 100,000 tries. it shouldn't ever give up.
     * a magic number is an unsigned sparsely populated 64 bit integer. a number.
     * when this number is multiplied by a blocker set and we take the top few bits, we can
     * form a hash that can be used to directly map any blocker set to it's attack set.
     * so this magic number needs to be very specific, because it needs to perfectly pseudo-randomly
     * shuffle bits around so they end up in the perfect hashing pattern
     * to be mapped with the desired collisions.
     */
    int tries = 1000000;
    while (tries--)
    {
        // forget all the attacks we remembered while looking for overlaps
        for (int i = 0; i < maxPermutations; i++)
        {
            attacksSeen[i] = 0;
        }
        // random64() returns a binary number where each bit has a 1:2 chance of being set
        // so by doing 3 consecutive &, we take that probability to 1:8 per bit. with a more sparse magic
        // number, we are going to find a magic number that works faster.
        uint64_t magic = random64() & random64() & random64();

        // go through every permutation of possible blocker boards (ways this sliding piece could be blocked)
        for (int permutation = 0; permutation < numPermutations; permutation++)
        {
            // figure out the hash value for this permutation
            uint16_t hash = blockers[permutation] * magic >> (isCardinal ? 52 : 55);
            // if we have not encountered this hash
            if (!attacksSeen[hash])
            {
                // we want to remember the hash and the attack set later so we know if we have an undesirable hash
                // collision. the type of undesirable collision is there being multiple occupancies that produce different
                // attack sets but still hash to the same magic index.
                attacksSeen[hash] = attacks[permutation];
            }
            // if we have seen this blocker hash (and the resulting attack set) before,
            // make sure it has a matching attack set to the one we are looking at now.
            // because otherwise, this number isn't magic.
            else if (attacksSeen[hash] != attacks[permutation])
            {
                // the attack sets don't match! the same blocker hash has been seen with two different
                // attack sets. therefore, this number will not work to produce a perfect hashing algorithm.
                magic = 0;
                // so try again
                break;
            }
        }
        // if we found a magic number
        if (magic)
        {
            // go through every blocker permutation
            for (int i = 0; i < maxPermutations; i++)
            {
                // save the attacks we saw for this hash and save them to the corresponding attack table
                if (isCardinal)
                {
                    cardinalAttacks[square][i] = attacksSeen[i];
                }
                else
                {
                    ordinalAttacks[square][i] = attacksSeen[i];
                }
            }

            return magic;
        }
    }
    // if we ran out of tries - program failure ):
    // this should never happen
    std::cerr << (isCardinal ? "Cardinal" : "Ordinal") << " magic number generation failed on square " << square << std::endl;
    assert(false);
}

/*
 * get a bitboard of squares that could block a rook on a given square.
 * this includes all the squares along the attack ray of the rook, but not the final square in the ray.
 * leaving out the final square in the ray is crucially important to the success of this "magic number" technique.
 * every time we have one less square in the blocker set, we divide the number of permutations by two. so it is
 * worth it to not include the final squares in the ray because they can never block us and therefore we drastically
 * minimise the number of permutations we have to hash in the worst case scenarios
 *
 * this takes the table size for rooks from 2^14 (16,384) to 2^12 (4,096)
 */
uint64_t MoveGen::getRookBlockers(uint8_t from)
{
    int row = from / 8;
    int col = from % 8;
    // squares at the end of the cardinal rays
    uint64_t endpoints = 0;
    endpoints |= boardOf(col); // north
    endpoints |= boardOf(row * 8 + 7); // east
    endpoints |= boardOf(56 + col); // south
    endpoints |= boardOf(from - col); // west

    // the endpoint squares don't block the rook
    return getRookAttacks(from, endpoints, false);
}

/*
 * iteratively generate a bitboard of rook attacks for a given square and blocker bitboard
 *
 * blockers is a bitboard showing where blocking pieces are.
 * if captures is true, then these blocker bits are included in the attack set.
 *
 * this function is slow. it is only used to populate attack tables and magic numbers for sliding piece calculation on program start
 */
uint64_t MoveGen::getRookAttacks(uint8_t from, uint64_t blockers, bool captures)
{
    uint64_t attacks = 0;
    int rowFrom = from / 8;
    int colFrom = from % 8;

    // north
    for (int row = rowFrom - 1; row >= 0; row--)
    {
        uint64_t attack = boardOf(row * 8 + colFrom);
        if (captures)
        {
            attacks |= attack;
        }
        if (attack & blockers)
        {
            break;
        }
        attacks |= attack;
    }
    // east
    for (int col = colFrom + 1; col < 8; col++)
    {
        uint64_t attack = boardOf(rowFrom * 8 + col);
        if (captures)
        {
            attacks |= attack;
        }
        if (attack & blockers)
        {
            break;
        }
        attacks |= attack;
    }
    // south
    for (int row = rowFrom + 1; row < 8; row++)
    {
        uint64_t attack = boardOf(row * 8 + colFrom);
        if (captures)
        {
            attacks |= attack;
        }
        if (attack & blockers)
        {
            break;
        }
        attacks |= attack;
    }
    // west
    for (int col = colFrom - 1; col >= 0; col--)
    {
        uint64_t attack = boardOf(rowFrom * 8 + col);
        if (captures)
        {
            attacks |= attack;
        }
        if (attack & blockers)
        {
            break;
        }
        attacks |= attack;
    }

    return attacks;
}

/*
 * we need to exclude the ray endpoints of the bishops as well - this takes the table
 * size for bishops from 2^13 to 2^9. also, the reduced table size makes finding magic numbers
 * in a short amount of time much more realistic
 */
uint64_t MoveGen::getBishopBlockers(uint8_t from)
{
    // I am getting lucky here - the endpoints of ordinal rays are always just the edge squares for bishops
    // this is not the case for rooks because they can slide along the outer edges
    return getBishopAttacks(from, OUTER_SQUARES, false);
}

/*
 * iteratively generate a bitboard of bishop attacks for a given square and blocker bitboard
 *
 * blockers is a bitboard showing where blocking pieces are.
 * if captures is true, then these blocker bits are included in the attack set.
 *
 * this function is slow. it is only used to populate attack tables and magic numbers for sliding piece calculation on program start
 */
uint64_t MoveGen::getBishopAttacks(uint8_t from, uint64_t blockers, bool captures)
{
    uint64_t attacks = 0;

    // northeast ray
    int row = from / 8 - 1;
    int col = from % 8 + 1;
    while (isOnBoard(row, col))
    {
        uint64_t attacked = boardOf(row * 8 + col);
        if (captures)
        {
            attacks |= attacked;
        }
        if (attacked & blockers)
        {
            break;
        }
        attacks |= attacked;
        row--;
        col++;
    }
    // southeast ray
    row = from / 8 + 1;
    col = from % 8 + 1;
    while(isOnBoard(row, col))
    {
        uint64_t attacked = boardOf(row * 8 + col);
        if (captures)
        {
            attacks |= attacked;
        }
        if (attacked & blockers)
        {
            break;
        }
        attacks |= attacked;
        row++;
        col++;
    }
    // southwest ray
    row = from / 8 + 1;
    col = from % 8 - 1;
    while(isOnBoard(row, col))
    {
        uint64_t attacked = boardOf(row * 8 + col);
        if (captures)
        {
            attacks |= attacked;
        }
        if (attacked & blockers)
        {
            break;
        }
        attacks |= attacked;
        row++;
        col--;
    }
    // northwest ray
    row = from / 8 - 1;
    col = from % 8 - 1;
    while(isOnBoard(row, col))
    {
        uint64_t attacked = boardOf(row * 8 + col);
        if (captures)
        {
            attacks |= attacked;
        }
        if (attacked & blockers)
        {
            break;
        }
        attacks |= attacked;
        row--;
        col--;
    }

    return attacks;
}

bool MoveGen::isKingInCheck(bool isEngine)
{
    if (isEngine)
    {
        return !isSafeSquare<true>(getLeastSquare(position->pieces[ENGINE_KING]));
    }
    else
    {
        return !isSafeSquare<false>(getLeastSquare(position->pieces[PLAYER_KING]));
    }
}

/*
 * all the code below is performance sensitive. it is what is running during the search.
 */
void MoveGen::generateEngineMoves()
{
    getBlockerSquares<true>();
    getCardinalPins<true>();
    getOrdinalPins<true>();

    generated.clear();
    generatePawnMoves<true>();
    generateKnightMoves<true>();
    generateKingMoves<true>();
    generateBishopMoves<true>();
    generateRookMoves<true>();
    generateQueenMoves<true>();
}

void MoveGen::generatePlayerMoves()
{
    getBlockerSquares<false>();
    getCardinalPins<false>();
    getOrdinalPins<false>();

    generated.clear();
    generatePawnMoves<false>();
    generateKnightMoves<false>();
    generateKingMoves<false>();
    generateBishopMoves<false>();
    generateRookMoves<false>();
    generateQueenMoves<false>();
}

// this is the function we call from the outside to get the generated moves.
// it also sorts sort the moves from best to worst, in order to further prune the search tree
// the moves go in this order: captures (winning), captures (losing), pawn moves
std::vector<Board::Move> MoveGen::getSortedMoves()
{
    std::vector<Board::Move> sorted;

    while (!generated.empty())
    {
        int score = 0;
        int bestScore = -1;
        int bestMoveIndex = 0;

        for (int index = 0; index < generated.size(); index++)
        {
            Board::Move move = generated[index];
            // captures first
            if (move.captured)
            {
                // sort winning captures before losing captures (PxQ before QxP)
                // this number will always be at least the value of a pawn
                score = PIECE_VALUES[ENGINE_QUEEN] + PIECE_VALUES[move.captured] - PIECE_VALUES[move.moving];
            }
            // pawn moves last
            else if (move.moving == PLAYER_PAWN || move.moving == ENGINE_PAWN)
            {
                score = 0;
            }
            if (score > bestScore)
            {
                bestScore = score;
                bestMoveIndex = index;
            }
        }
        sorted.push_back(generated[bestMoveIndex]);
        generated.erase(generated.begin() + bestMoveIndex);
    }
    std::reverse(sorted.begin(), sorted.end());
    return sorted;
}


template<bool isEngine>
inline void MoveGen::generateQueenMoves()
{
    // the queens on the board. there could be 9 of them, for all I know.
    uint64_t queens = board->position.pieces[isEngine ? ENGINE_QUEEN : PLAYER_QUEEN];
    while (queens)
    {
        // look at each queen
        uint64_t queen = popLeastBitboard(queens);
        uint8_t from = getLeastSquare(queen);

        uint64_t moves = 0;
        // if we are unpinned horizontal/vertical
        if (queen & ~cardinalPins)
        {
            // generate ordinal sliding moves
            // figure out the pieces blocking the ordinal rays from the queen
            uint64_t ordinalBlockers = board->occupiedSquares & ordinals[from].blockers;
            // add the ordinal moves
            moves |= ordinalAttacks[from][ordinalBlockers * ordinals[from].magic >> 55];

            // if we are pinned diagonally
            if (queen & ordinalPins)
            {
                // make sure we don't leave the diagonal pin
                moves &= ordinalPins;
            }
        }
        // if we are unpinned diagonally
        if (queen & ~ordinalPins)
        {
            // generate cardinal sliding moves
            // figure out the pieces blocking the cardinal rays from the queen
            uint64_t cardinalBlockers = board->occupiedSquares & cardinals[from].blockers;
            // add the ordinal moves
            moves |= cardinalAttacks[from][cardinalBlockers * cardinals[from].magic >> 52];

            // if we are pinned horizontal/vertical
            if (queen & cardinalPins)
            {
                // make sure we don't leave the horizontal/vertical pin
                moves &= cardinalPins;
            }
        }
        // make sure the queen doesn't capture her own pieces
        moves &= isEngine ? board->playerOrEmpty : board->engineOrEmpty;
        // make sure we don't leave our king in check
        moves &= blockerSquares;

        // look at each queen move
        while (moves)
        {
            // add the queen move with a possible capture
            uint8_t to = popLeastSquare(moves);
            generated.push_back(Board::Move{
                Board::NORMAL,
                from,
                to,
                isEngine ? ENGINE_QUEEN : PLAYER_QUEEN,
                isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)});
        }
    }
}

template<bool isEngine>
inline void MoveGen::generateRookMoves()
{
    // rooks we want to generate sliding moves for - we don't have any moves if we are diagonally pinned
    uint64_t rooks = board->position.pieces[isEngine ? ENGINE_ROOK : PLAYER_ROOK];
    // don't generate moves for diagonally pinned rooks
    rooks &= ~ordinalPins;
    while (rooks)
    {
        uint8_t from = popLeastSquare(rooks);
        // create a bitboard that are all the pieces blocking the movement of this rook
        uint64_t blockers = board->occupiedSquares & cardinals[from].blockers;
        // lookup the rook moves.
        uint64_t moves = cardinalAttacks[from][blockers * cardinals[from].magic >> 52];
        // don't let us capture our own pieces
        moves &= (isEngine ? board->playerOrEmpty : board->engineOrEmpty);
        // make sure we don't leave our king in check
        moves &= blockerSquares;
        // make sure we dont break a horizontal/vertical pin
        if (boardOf(from) & cardinalPins)
        {
            // don't allow moves that are not on the line of the pin
            moves &= cardinalPins;
        }
        // look at each rook move
        while (moves)
        {
            // add the move with a possible capture
            uint8_t to = popLeastSquare(moves);
            generated.push_back(Board::Move{
                    Board::NORMAL,
                    from,
                    to,
                    isEngine ? ENGINE_ROOK : PLAYER_ROOK,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
            });
        }
    }
}

template<bool isEngine>
inline void MoveGen::generateBishopMoves()
{
    // the bishops we are generating moves for
    uint64_t bishops = board->position.pieces[isEngine ? ENGINE_BISHOP : PLAYER_BISHOP];
    // don't generate moves for horizontal/vertical pinned bishops
    bishops &= ~cardinalPins;
    while (bishops)
    {
        // look at each bishop
        uint8_t from = popLeastSquare(bishops);
        // create a bitboard representing the pieces that could block a bishop on this square
        uint64_t blockers = board->occupiedSquares & ordinals[from].blockers;
        // create a 9 bit hash and use the hash to index the attack table.
        uint64_t moves = ordinalAttacks[from][blockers * ordinals[from].magic >> 55];
        // make sure we don't capture our own pieces
        moves &= (isEngine ? board->playerOrEmpty : board->engineOrEmpty);
        // make sure we don't leave our king in check
        moves &= blockerSquares;
        // make sure we dont break a diagonal pin
        if (boardOf(from) & ordinalPins)
        {
            // don't allow moves that are not on the line of the pin
            moves &= ordinalPins;
        }
        // look at each bishop move
        while (moves)
        {
            // add each move with a possible capture
            uint8_t to = popLeastSquare(moves);
            generated.push_back(Board::Move{
                Board::NORMAL,
                    from,
                    to,
                    isEngine ? ENGINE_BISHOP : PLAYER_BISHOP,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)});
        }
    }
}

template<bool isEngine>
inline void MoveGen::generateKnightMoves()
{
    // the knights we want to generate moves for.
    uint64_t knights = position->pieces[isEngine ? ENGINE_KNIGHT : PLAYER_KNIGHT];
    // don't generate moves for pinned knights
    knights &= ~(cardinalPins | ordinalPins);
    while (knights)
    {
        uint8_t from = popLeastSquare(knights);
        // get the knight moves, and make sure we don't capture our own pieces
        uint64_t moves = KNIGHT_MOVES[from] & (isEngine ? board->playerOrEmpty : board->engineOrEmpty);
        // make sure we don't leave our king in check
        moves &= blockerSquares;
        while (moves)
        {
            // add each move with its possible capture
            uint8_t to = popLeastSquare(moves);
            generated.push_back(Board::Move{
                Board::NORMAL,
                    from,
                    to,
                    isEngine ? ENGINE_KNIGHT : PLAYER_KNIGHT,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
            });
        }
    }
}

template<bool isEngine>
inline void MoveGen::generateKingMoves()
{
    uint8_t from = getLeastSquare(board->position.pieces[isEngine ? ENGINE_KING : PLAYER_KING]);
    // get the king's moves, and make sure it did not capture one of its own pieces
    uint64_t moves = KING_MOVES[from] & (isEngine ? board->playerOrEmpty : board->engineOrEmpty);
    // don't let the king walk onto attacked squares
    uint64_t safeMoves = 0;
    while (moves)
    {
        uint8_t square = popLeastSquare(moves);
        if (isSafeSquare<isEngine>(square))
        {
            safeMoves |= boardOf(square);
        }
    }

    // if we have not lost the right to castle queenside
    if (isEngine ? position->engineCastleQueenside : position->playerCastleQueenside)
    {
        // look at the entire path between rook and king
        uint64_t path = isEngine ? ENGINE_QUEENSIDE_CASTLE : PLAYER_QUEENSIDE_CASTLE;
        // take this "extra" square that is in between the destination square and rook into consideration.
        // this only happens when we are castling queenside. we don't care about the check safety of the extra square
        // because the square is not on the path the king takes
        uint64_t extraSquare = isEngine ? ENGINE_QUEENSIDE_DESTINATION : PLAYER_QUEENSIDE_DESTINATION;
        path |= ENGINE_IS_WHITE ? extraSquare << 1 : extraSquare >> 1;
        // make sure there are three empty squares. if any of these squares have pieces on them,
        // we cannot castle anyway so it is not worth it do square safety validation along the castling path
        path &= board->emptySquares;
        // if there are no pieces in the way
        if (countSetBits(path) == 3)
        {
            // there are no pieces in the way of queenside castling.
            // but let's also make sure we are not castling out of check, through check, or into check
            path = isEngine ? ENGINE_QUEENSIDE_CASTLE : PLAYER_QUEENSIDE_CASTLE;
            while (path)
            {
                // if we find an unsafe square along the path, we cannot castle
                if (!isSafeSquare<isEngine>(popLeastSquare(path)))
                {
                    break;
                }
                // we looked at every square along the path and determined they are all safe to castle over
                if (!path)
                {
                    // add the queenside castling move
                    safeMoves |= isEngine ? ENGINE_QUEENSIDE_DESTINATION : PLAYER_QUEENSIDE_DESTINATION;
                }
            }
        }
    }

    // if we have not lost the right to castle kingside
    if (isEngine ? position->engineCastleKingside : position->playerCastleKingside)
    {
        // look at the entire path between rook and king
        uint64_t path = isEngine ? ENGINE_KINGSIDE_CASTLE : PLAYER_KINGSIDE_CASTLE;
        // make sure there are two empty squares. if these squares have pieces on them,
        // we cannot castle anyway so it is not worth it to do check validation on the squares
        path &= board->emptySquares;
        if (countSetBits(path) == 2)
        {
            // there are no pieces in the way of castling.
            // now let's make sure we are not castling out of check, through check, or into check
            path = isEngine ? ENGINE_KINGSIDE_CASTLE : PLAYER_KINGSIDE_CASTLE;
            // look through each square along the king's path
            while (path)
            {
                // if a square is in check, we cannot castle, so stop looking
                if (!isSafeSquare<isEngine>(popLeastSquare(path)))
                {
                    break;
                }
                // if we looked at every square along the path and determined they are all safe to castle over
                if (!path)
                {
                    // add the kingside castling move
                    safeMoves |= isEngine ? ENGINE_KINGSIDE_DESTINATION : PLAYER_KINGSIDE_DESTINATION;
                }
            }
        }
    }

    while (safeMoves)
    {
        // add the king move with its possible capture
        uint8_t to = popLeastSquare(safeMoves);
        generated.push_back(Board::Move{
            Board::NORMAL,
            from,
            to,
            isEngine ? ENGINE_KING : PLAYER_KING,
            isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)});
    }
}


template<bool isEngine>
inline void MoveGen::generatePawnMoves()
{
    // the pawns we are generating moves for
    uint64_t pawns = isEngine ? position->pieces[ENGINE_PAWN] : position->pieces[PLAYER_PAWN];
    // push the pawn up one square if we can
    // also, don't generate pawn pushes for diagonally pinned pawns
    uint64_t singlePush = (isEngine ? (pawns & ~ordinalPins) << 8 : (pawns & ~ordinalPins) >> 8) & board->emptySquares;
    // push the pawn up two squares, if it is a legal move
    uint64_t doublePush = (isEngine ? (singlePush & RANK2) << 8 : (singlePush & RANK5) >> 8) & board->emptySquares;

    // make sure we have to block the checking piece if we are in check
    singlePush &= blockerSquares;
    doublePush &= blockerSquares;

    // go through each one square pawn move
    while (singlePush)
    {
        uint8_t to = popLeastSquare(singlePush);
        uint8_t from = to + (isEngine ? -8 : 8);

        uint64_t squareTo = boardOf(to);

        // exclude push moves that should have been horizontal/vertical pinned
        if ((boardOf(from) & cardinalPins) && !(squareTo & cardinalPins))
        {
            continue;
        }

        // if this pawn push is a promotion, generate all promotion types.
        if (squareTo & (isEngine ? RANK7 : RANK0))
        {
            for (int promotionChoice = 0; promotionChoice < 4; promotionChoice++)
            {
                generated.push_back(Board::Move{
                    (Board::MoveType)promotionChoice,
                    from,
                    to,
                    isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                    NONE
                });
            }
        }
        // otherwise, just generate the pawn push move
        else
        {
            generated.push_back(Board::Move{
                Board::NORMAL,
                from,
                to,
                isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                NONE
            });
        }
    }

    // go through each two square pawn move
    while (doublePush)
    {
        // figure out where we came from
        uint8_t to = popLeastSquare(doublePush);
        uint8_t from = to + (isEngine ? -16 : 16);
        // exclude push moves that should have been horizontal/vertical pinned
        if ((boardOf(from) & cardinalPins) && !(boardOf(to) & cardinalPins))
        {
            continue;
        }
        // add the pawn move
        generated.push_back(Board::Move{
                Board::NORMAL,
                from,
                to,
                isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                NONE
        });
    }

    // now we want to generate all types of pawn captures.
    // there is no case where a horizontal/vertical pinned pawn is able to capture.
    // so get rid of horizontal/vertical pinned pawns right away
    pawns &= ~cardinalPins;

    // generate possible left captures. make sure we don't shift over the edge
    uint64_t leftAttacks = isEngine ? (pawns & ~FILE7) << 9 : (pawns & ~FILE0) >> 9;
    // only consider captures
    leftAttacks &= isEngine ? board->playerPieces : board->enginePieces;
    // make sure we have to capture a checking piece
    leftAttacks &= blockerSquares;
    // look at each of the left captures
    while (leftAttacks)
    {
        // figure out where we came from
        uint8_t to = popLeastSquare(leftAttacks);
        uint8_t from = to + (isEngine ? -9 : 9);

        uint64_t squareTo = boardOf(to);
        uint64_t squareFrom = boardOf(from);

        // exclude captures that should have been diagonally pinned
        if (squareFrom & ordinalPins && !(squareTo & ordinalPins))
        {
            continue;
        }

        // if this pawn capture comes with a promotion
        if (squareTo & (isEngine ? RANK7 : RANK0))
        {
            // generate all possible promotion moves
            for (int promotionChoice = 0; promotionChoice < 4; promotionChoice++)
            {
                generated.push_back(Board::Move{
                    (Board::MoveType)promotionChoice,
                    from,
                    to,
                    isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
                });
            }
        }
        // if we are not promoting
        else
        {
            // just add the pawn capture
            generated.push_back(Board::Move{
                    Board::NORMAL,
                    from,
                    to,
                    isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
            });
        }
    }
    // generate right attacks. make sure we don't shift over the edge
    uint64_t rightAttacks = isEngine ? (pawns & ~FILE0) << 7 : (pawns & ~FILE7) >> 7;
    // only consider captures
    rightAttacks &= isEngine ? board->playerPieces : board->enginePieces;
    // make sure we have to capture a checking piece
    rightAttacks &= blockerSquares;
    // look at each of the right captures
    while (rightAttacks)
    {
        // figure out where we came from
        uint8_t to = popLeastSquare(rightAttacks);
        uint8_t from = to + (isEngine ? -7 : 7);

        uint64_t squareTo = boardOf(to);
        uint64_t squareFrom = boardOf(from);

        // exclude captures that should have been diagonally pinned
        if (squareFrom & ordinalPins && !(squareTo & ordinalPins))
        {
            continue;
        }
        // if this capture comes with a promotion
        if (squareTo & (isEngine ? RANK7 : RANK0))
        {
            // generate every possible promotion choice as a capture
            for (int promotionChoice = 0; promotionChoice < 4; promotionChoice++)
            {
                generated.push_back(Board::Move{
                        (Board::MoveType)promotionChoice,
                        from,
                        to,
                        isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                        isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
                });
            }
        }
        // if this capture was not a promotion
        else
        {
            // just add the capture move
            generated.push_back(Board::Move{
                    Board::NORMAL,
                    from,
                    to,
                    isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                    isEngine ? board->getPlayerPieceType(to) : board->getEnginePieceType(to)
            });
        }
    }

    // if we should try to generate en passant capture moves.
    if (position->enPassantCapture)
    {
        // first, restrict the pawns to the correct rank
        pawns &= isEngine ? RANK4 : RANK3;

        // figure out the en passant capture square. get rid of en passant captures that leave the king in check
        uint64_t rightEnPassant = position->enPassantCapture & (isEngine ? pawns >> 1 : pawns << 1) & blockerSquares;
        uint64_t leftEnPassant = position->enPassantCapture & (isEngine ? pawns << 1 : pawns >> 1) & blockerSquares;

        // we are going to need to do some special pin detection to make this work.
        // this is because a pin is defined as the line between an attacking enemy piece and our king, with ONE of our pieces in between.
        // in an en passant scenario, there can be TWO pieces in the line of a horizontal pin, because the captured en passant pawn
        // and the capturing en passant pawn both disappear from the rank when the move is played
        // because of this, this type of horizontal en passant pin is not picked up by the pin generation

        // if there is a right en passant capture
        if (rightEnPassant)
        {
            // generate the right en passant capture
            uint8_t from = getLeastSquare(isEngine ? rightEnPassant << 1 : rightEnPassant >> 1);
            uint8_t to = from + (isEngine ? 7 : -7);

            uint64_t blockers = cardinals[from].blockers & board->occupiedSquares & ~(position->enPassantCapture);
            // get the horizontal attacks from the pawn we are capturing with
            uint64_t pin = cardinalAttacks[from][blockers * cardinals[from].magic >> 52] & (isEngine ? RANK4 : RANK3);
            pin &= position->pieces[isEngine ? ENGINE_KING : PLAYER_KING] | // look for our own king along the attack
                   position->pieces[isEngine ? PLAYER_QUEEN : ENGINE_QUEEN] |  // look for enemy queens
                   position->pieces[isEngine ? PLAYER_ROOK : ENGINE_ROOK]; // look for enemy rooks


            if ((boardOf(from) & ordinalPins) && !(boardOf(to) & ordinalPins))
            {
                // we can't add this en passant capture. moving the pawn in this way breaks a diagonal pin
            }
            // if we are not horizontally pinned
            else if (countSetBits(pin) != 2)
            {
                // add the right en passant capture
                generated.push_back(Board::Move{
                        Board::EN_PASSANT,
                        from,
                        to,
                        isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                        isEngine ? PLAYER_PAWN : ENGINE_PAWN
                });
            }
        }
        // if there is a left en passant capture
        if (leftEnPassant)
        {
            // generate the left en passant capture
            uint8_t from = getLeastSquare(isEngine ? leftEnPassant >> 1 : leftEnPassant << 1);
            uint8_t to = from + (isEngine ? 9 : -9);

            uint64_t blockers = cardinals[from].blockers & board->occupiedSquares & ~(position->enPassantCapture);
            // get the horizontal attacks from the pawn we are capturing with
            uint64_t pin = cardinalAttacks[from][blockers * cardinals[from].magic >> 52] & (isEngine ? RANK4 : RANK3);
            pin &= position->pieces[isEngine ? ENGINE_KING : PLAYER_KING] | // look for our own king along the attack
                   position->pieces[isEngine ? PLAYER_QUEEN : ENGINE_QUEEN] |  // look for enemy queens
                   position->pieces[isEngine ? PLAYER_ROOK : ENGINE_ROOK]; // look for enemy rooks

            if ((boardOf(from) & ordinalPins) && !(boardOf(to) & ordinalPins))
            {
                // we can't add this en passant capture. moving the pawn in this way breaks a diagonal pin
            }
            // if we are not horizontally pinned
            else if (countSetBits(pin) != 2)
            {
                // add the left en passant capture
                generated.push_back(Board::Move{
                        Board::EN_PASSANT,
                        from,
                        to,
                        isEngine ? ENGINE_PAWN : PLAYER_PAWN,
                        isEngine ? PLAYER_PAWN : ENGINE_PAWN
                });
            }
        }

    }

}

/*
 * populate the blockerSquares bitboard with squares on the board we could move to in order to
 * block an opponent's check or capture the checking piece. if there are no checkers, all squares are set to 1.
 * if there are multiple checkers, all squares are set to 0 because no piece can block a double check
 *
 * my general strategy for this is to start at the king's square, and search outward in all movement types
 * we can logical AND the resulting board with enemy pieces and get a bitboard of the pieces putting the king in check.
 * if there is only one checker, we logical AND the piece's attack bitboard with the king's scan bitboard to get the
 * squares we could  block the piece giving check to our king. then we logical OR the resulting board with the attacking piece
 * board to get the final set of squares we can move to in order to capture or block the checking piece
 *
 */
template<bool isEngine>
inline void MoveGen::getBlockerSquares()
{
    uint64_t king = isEngine ? position->pieces[ENGINE_KING] : position->pieces[PLAYER_KING];
    uint8_t kingSquare = getLeastSquare(king);

    // figure out the occupancies of both movement types for the king's square
    uint64_t cardinalBlockers = board->occupiedSquares & cardinals[kingSquare].blockers;
    uint64_t ordinalBlockers = board->occupiedSquares & ordinals[kingSquare].blockers;

    // lookup the sliding movement attack rays from the king
    uint64_t cardinalAttackRays = cardinalAttacks[kingSquare][cardinalBlockers * cardinals[kingSquare].magic >> 52];
    uint64_t ordinalAttackRays = ordinalAttacks[kingSquare][ordinalBlockers * ordinals[kingSquare].magic >> 55];

    // bitboard of enemy rooks or queens attacking our king
    uint64_t cardinalAttackers = cardinalAttackRays & (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_ROOK]
                                                                : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_ROOK]);
    // bitboard of the enemy bishops or queens attacking our king
    uint64_t ordinalAttackers = ordinalAttackRays & (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_BISHOP]
                                                              : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_BISHOP]);
    // bitboard of the sliding pieces attacking our king
    uint64_t attackers = cardinalAttackers | ordinalAttackers;
    // add possible enemy knights attacking our king
    attackers |= KNIGHT_MOVES[kingSquare] & (isEngine ? position->pieces[PLAYER_KNIGHT] : position->pieces[ENGINE_KNIGHT]);
    // add possible left pawn attacks
    attackers |= (isEngine ? (king & ~FILE7) << 9 & position->pieces[PLAYER_PAWN] : (king & ~FILE0) >> 9 & position->pieces[ENGINE_PAWN]);
    // add possible right pawn attacks
    attackers |= (isEngine ? (king & ~FILE0) << 7 & position->pieces[PLAYER_PAWN] : (king & ~FILE7) >> 7 & position->pieces[ENGINE_PAWN]);

    // if our king is not in check
    if (!attackers)
    {
        // every square successfully stops check because we are not in check
        blockerSquares = FILLED_BOARD;
    }
    // if the king is checked by a single piece
    else if (countSetBits(attackers) == 1)
    {
        uint8_t attacker;
        // if the checking piece gives a cardinal attack ray
        if (cardinalAttackers)
        {
            attacker = getLeastSquare(cardinalAttackers);
            cardinalBlockers = board->occupiedSquares & cardinals[attacker].blockers;
            // figure out which squares would stop the king from being put in check
            cardinalAttackRays &= cardinalAttacks[attacker][cardinalBlockers * cardinals[attacker].magic >> 52];
            // add on the capture of the attacking piece and remember these squares
            blockerSquares = cardinalAttackRays | attackers;
        }
        // if the checking piece gives an ordinal attack ray
        else if (ordinalAttackers)
        {
            attacker = getLeastSquare(ordinalAttackers);
            ordinalBlockers = board->occupiedSquares & ordinals[attacker].blockers;
            // figure out which squares would stop the king from being put in check
            ordinalAttackRays &= ordinalAttacks[attacker][ordinalBlockers * ordinals[attacker].magic >> 55];
            // add on the capture of the attacking piece and remember these squares
            blockerSquares = ordinalAttackRays | attackers;
        }
        else
        {
            // if the checking piece is not a sliding piece, the only way to block
            // or capture the check is by capturing the checking piece.
            blockerSquares = attackers;
        }
    }
    // if the king is checked by multiple pieces
    else
    {
        // in the case of double check, there are no squares that are legal for blocks or captures
        // this is because you cannot block double check or capture two pieces at once.
        // you have to move the king, otherwise it is checkmate
        blockerSquares = 0;
    }
}

/*
 * a function that updates the ordinalPins bitboard.
 *
 * a pin on the ordinalPins board (there may be multiple) is the attack ray from the king to
 * the pinning piece. we can generate this attack ray more easily by removing the pinned piece
 * from the occupied bitboard before hashing it to index the ordinal moves
 */
template<bool isEngine>
inline void MoveGen::getOrdinalPins()
{
    ordinalPins = 0;

    uint8_t king = getLeastSquare(position->pieces[isEngine ? ENGINE_KING : PLAYER_KING]);

    // blockers along the ordinal rays of the king to hash into ordinal attack index
    uint64_t ordinalBlockers = board->occupiedSquares & ordinals[king].blockers;
    // get the squares that could have a diagonally pinned piece on it
    uint64_t possiblyPinned = ordinalAttacks[king][ordinalBlockers * ordinals[king].magic >> 55];
    possiblyPinned &= (isEngine ? board->enginePieces : board->playerPieces);

    // remove the supposedly pinned pieces from the blocker set
    ordinalBlockers &= ~possiblyPinned;
    // scan diagonally from the king again in all directions, but using the new blocker bitboard
    // that excludes friendly pieces that have the potential to be pinned. this allows us to
    // scan "through" the possibly pinned piece, looking for the pinning piece that may or may not exist
    uint64_t pins = ordinalAttacks[king][ordinalBlockers * ordinals[king].magic >> 55];
    // bitboard of the enemy pieces that are pinning the pinned pieces
    // because we found an enemy along the attack ray through the removed friendly piece,
    // we know the piece we removed was actually pinned
    uint64_t pinning = pins & (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_BISHOP]
                                  : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_BISHOP]);
    // go through each square that might have a diagonally pinning enemy piece
    while (pinning)
    {
        uint8_t pinningSquare = popLeastSquare(pinning);
        // recalculate blocker bitboard before hashing movement of pinning piece. maybe verify that this is required later.
        ordinalBlockers = board->occupiedSquares & ordinals[pinningSquare].blockers & ~possiblyPinned;
        // generate all ordinal attack rays coming out of the pinning piece
        uint64_t pin = ordinalAttacks[pinningSquare][ordinalBlockers * ordinals[pinningSquare].magic >> 55];
        // add this pin to the ordinal pins bitboard
        ordinalPins |= pins & pin;
        // don't forget to add the capturing move -- we can capture a pinning piece while pinned.
        ordinalPins |= boardOf(pinningSquare);
    }
}

/*
 * a function that updates the cardinalPins bitboard.
 *
 * a pin on the cardinalPins board (there may be multiple) is the attack ray from the king to
 * the pinning piece. we can generate this attack ray more easily by removing the pinned piece
 * from the occupied bitboard before hashing it to index the ordinal moves
 */
template<bool isEngine>
inline void MoveGen::getCardinalPins()
{
    cardinalPins = 0;

    uint8_t king = getLeastSquare(position->pieces[isEngine ? ENGINE_KING : PLAYER_KING]);

    // blockers along the cardinal rays of the king to hash into cardinal attack index
    uint64_t cardinalBlockers = board->occupiedSquares & cardinals[king].blockers;
    // get the squares that could have a horizontal/vertical pinned piece on it
    uint64_t possiblyPinned = cardinalAttacks[king][cardinalBlockers * cardinals[king].magic >> 52];
    possiblyPinned &= (isEngine ? board->enginePieces : board->playerPieces);

    // remove the supposedly pinned pieces from the blocker set
    cardinalBlockers &= ~possiblyPinned;
    // scan diagonally from the king again in all directions, but using the new blocker bitboard
    // that excludes friendly pieces that have the potential to be pinned. this allows us to
    // scan "through" the possibly pinned piece, looking for the pinning piece that may or may not exist
    uint64_t pins = cardinalAttacks[king][cardinalBlockers * cardinals[king].magic >> 52];
    // bitboard of the enemy pieces that are pinning the pinned pieces
    // because we found an enemy along the attack ray through the removed friendly piece,
    // we know the piece we removed was actually pinned
    uint64_t pinning = pins & (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_ROOK]
                                        : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_ROOK]);

    // go through each square that might have a diagonally pinning enemy piece
    while (pinning)
    {
        uint8_t pinningSquare = popLeastSquare(pinning);
        // recalculate blocker bitboard before hashing movement of pinning piece.
        cardinalBlockers = board->occupiedSquares & cardinals[pinningSquare].blockers & ~possiblyPinned;
        // generate all ordinal attack rays coming out of the pinning piece
        uint64_t pin = cardinalAttacks[pinningSquare][cardinalBlockers * cardinals[pinningSquare].magic >> 52];
        // add this pin to the ordinal pins bitboard
        cardinalPins |= pins & pin;
        // don't forget to add the capturing move -- we can capture a pinning piece while pinned.
        cardinalPins |= boardOf(pinningSquare);
    }
}

/*
 * a function to tell whether a square is attacked. this is only used for checking
 * if the square a king is moving to (or castling out of or over) is checked by an opponent piece or not
 */
template<bool isEngine>
inline bool MoveGen::isSafeSquare(uint8_t square)
{
    uint64_t attacked = boardOf(square);

    // figure out the occupancies of both movement types for the desired square.
    uint64_t cardinalBlockers = board->occupiedSquares & cardinals[square].blockers;
    uint64_t ordinalBlockers = board->occupiedSquares & ordinals[square].blockers;

    // make sure to remove the king when calculating these squares. this
    // is so we cannot slide the king along the ray away from the attacker to evade check
    cardinalBlockers &= ~position->pieces[isEngine ? ENGINE_KING : PLAYER_KING];
    ordinalBlockers &= ~position->pieces[isEngine ? ENGINE_KING : PLAYER_KING];

    // lookup the sliding movement attack rays from the square
    uint64_t cardinalAttackers = cardinalAttacks[square][cardinalBlockers * cardinals[square].magic >> 52];
    uint64_t ordinalAttackers = ordinalAttacks[square][ordinalBlockers * ordinals[square].magic >> 55];

    // bitboard of enemy rooks or queens attacking our square
    cardinalAttackers &= (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_ROOK]
                                   : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_ROOK]);
    // bitboard of the enemy bishops or queens attacking our square
    ordinalAttackers &= (isEngine ? position->pieces[PLAYER_QUEEN] | position->pieces[PLAYER_BISHOP]
                                  : position->pieces[ENGINE_QUEEN] | position->pieces[ENGINE_BISHOP]);
    // bitboard of the sliding pieces attacking our square
    uint64_t attackers = cardinalAttackers | ordinalAttackers;
    // add possible enemy knights attacking our square
    attackers |= KNIGHT_MOVES[square] & (isEngine ? position->pieces[PLAYER_KNIGHT] : position->pieces[ENGINE_KNIGHT]);
    // add possible king attacks
    attackers |= KING_MOVES[square] & (isEngine ? position->pieces[PLAYER_KING] : position->pieces[ENGINE_KING]);
    // add possible left pawn attacks
    attackers |= (isEngine ? (attacked & ~FILE7) << 9 & position->pieces[PLAYER_PAWN] : (attacked & ~FILE0) >> 9 & position->pieces[ENGINE_PAWN]);
    // add possible right pawn attacks
    attackers |= (isEngine ? (attacked & ~FILE0) << 7 & position->pieces[PLAYER_PAWN] : (attacked & ~FILE7) >> 7 & position->pieces[ENGINE_PAWN]);

    return attackers == 0;
}
