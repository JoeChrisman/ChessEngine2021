//
// Created by Joe Chrisman on 2/24/22.
//

#include "Search.h"

Search::Search(MoveGen *generator)
{
    this->generator = generator;
    this->board = generator->board;

}

// make every possible engine move, and get a score for each move by
// doing a recursive depth first search. then return the highest score we found
int Search::maximize(int ply, int alpha, int beta)
{
    // if we have reached a leaf node in our search
    if (ply > SEARCH_DEPTH)
    {
        // return the evaluation score through the recursive callers above
        return evaluator.evaluate(board->position);
    }

    int bestScore = MIN_EVAL;

    generator->generateEngineMoves();
    std::vector<Board::Move> moves = generator->getSortedMoves();
    // if there are no moves, the engine is in checkmate or stalemate
    if (moves.empty())
    {
        // if our king is checked by our opponent
        if (generator->isKingInCheck(true))
        {
            // the engine is in checkmate. return a very low evaluation.
            // if we are deeper in the search, make that evaluation a little higher.
            // this way, the engine always chooses the longest mating line possible
            return MIN_EVAL + ply;
        }
        // otherwise, stalemate
        return 0;
    }

    for (Board::Move &move : moves)
    {
        Board::Position clone = board->position;
        // make a move for the engine
        board->makeMove<true>(move);

        // make the moves for the player
        int score = minimize(ply + 1, alpha, beta);
        if (score > bestScore)
        {
            bestScore = score;
        }

        // unmake the move
        board->position = clone;
        board->engineToMove = !board->engineToMove;

        if (bestScore > alpha)
        {
            alpha = bestScore;
        }
        if (beta <= alpha)
        {
            // no need to do more evaluating
            break;
        }
    }
    return bestScore;
}

// make every possible player move, and get a score for each move by
// doing a recursive depth first search. then return the lowest score we found
int Search::minimize(int ply, int alpha, int beta)
{
    // if we have reached a leaf node in our search
    if (ply > SEARCH_DEPTH)
    {
        // return the evaluation score through the recursive callers above
        return evaluator.evaluate(board->position);
    }

    int bestScore = MAX_EVAL;

    generator->generatePlayerMoves();
    std::vector<Board::Move> moves = generator->getSortedMoves();
    // if there are no moves, the player is in checkmate or stalemate
    if (moves.empty())
    {
        // if our king is checked by our opponent
        if (generator->isKingInCheck(false))
        {
            // the player is in checkmate. return a very high evaluation.
            // if we are deeper in the search, make that evaluation a little lower
            // this way, the engine will know to choose the fastest checkmate first
            return MAX_EVAL - ply;
        }
        // otherwise, stalemate
        return 0;
    }
    for (Board::Move &move : moves)
    {
        Board::Position clone = board->position;
        // make the move for the player
        board->makeMove<false>(move);
        int score = maximize(ply + 1, alpha, beta);
        if (score < bestScore)
        {
            bestScore = score;
        }
        // unmake the move
        board->position = clone;
        board->engineToMove = !board->engineToMove;

        if (bestScore < beta)
        {
            beta = bestScore;
        }
        if (beta <= alpha)
        {
            // no need to do any more searching
            break;
        }
    }
    return bestScore;
}


// play every possible move the engine could make.
// give each move a score, and choose the highest score.
// this move leads to the best play for the engine
Board::Move Search::getBestMove()
{
    auto start = std::chrono::steady_clock::now();
    Board::Move best;
    // start with the lowest score. look for the highest score possible
    // checkmate in 1 for the engine will return a score of MAX_EVAL - 1
    // the engine being checkmated in 1 will return a score of MIN_EVAL + 1
    int bestScore = MIN_EVAL;

    generator->generateEngineMoves();
    std::vector<Board::Move> moves = generator->getSortedMoves();
    // go through all the engine moves
    for (Board::Move &move : moves)
    {
        Board::Position clone = board->position;

        // make the move
        board->makeMove<true>(move);
        // get the score for the move by doing a recursive depth first search
        int score = minimize(1, MIN_EVAL, MAX_EVAL);
        std::cout << board->getMoveNotation(move) << ": " << score << std::endl;
        if (score > bestScore)
        {
            bestScore = score;
            best = move;
        }

        // unmake the move
        board->position = clone;
        board->engineToMove = !board->engineToMove;
    }
    auto end = std::chrono::steady_clock::now();
    auto difference = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << difference.count() << "ms elapsed.\n";

    return best;
}