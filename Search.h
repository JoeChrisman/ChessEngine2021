//
// Created by Joe Chrisman on 2/24/22.
//

#ifndef UNTITLED2_SEARCH_H
#define UNTITLED2_SEARCH_H

#include "MoveGen.h"

class Search
{
public:

    Search(MoveGen *generator);

    MoveGen *generator;
    Board *board;
    Evaluation evaluator;

    Board::Move getBestMove();
    int minimize(int ply, int alpha, int beta);
    int maximize(int ply, int alpha, int beta);

};


#endif //UNTITLED2_SEARCH_H
