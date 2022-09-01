//
// Created by Joe Chrisman on 2/23/22.
//

#ifndef UNTITLED2_CHESSGAME_H
#define UNTITLED2_CHESSGAME_H

#include "Search.h"

/*
 * this class handles things required to get the game running up on the screen.
 * it keeps track of pieces and squares and their locations on the screen
 *
 * it also keeps track of pieces we are dragging or moving.
 * a drag is when a player drags a piece with the cursor to move it.
 * a move is when the player clicks a piece and then clicks a destination square to move it there.
 * a "move" is an animation. a move can also be when the engine moves a piece.
 * the engine moves pieces, it does not drag them
 */

class ChessGame
{
public:

    ChessGame(SDL_Renderer *renderer);
    SDL_Renderer *renderer;

    /*
     * update the dragging piece to follow the user's cursor plus some offset
     * so the piece is dragged relative from where the user clicks on it
     */
    void onMouseMoved(int mouseX, int mouseY);

    /*
     * try to either pick up a piece and drag it, or move a selected piece to a square.
     * in order to move a selected piece to a square, the square clicked on must be marked as movable
     */
    void onMousePressed(int mouseX, int mouseY, bool isRightClick);

    /*
     *
     */
    void onMouseReleased(int mouseX, int mouseY);

    // move the moving piece little by little towards its destination
    void updateMovingPiece();
    // render the board gui
    void render();
    bool isAnimating();


private:


    /*
     * a bunch of data representing the chessboard.
     * this data is mostly bitboards - 64 bit unsigned integers for the pieces
     * bitboards are weak at answering questions like what piece resides on square x etc
     * so this board is not used for any UI purposes. it is purely for storing board data
     */
    Board *board;

    /*
     * a class to generate moves for us. we use the bitboards in the position struct
     * to generate moves efficiently. this generator is given the board pointer so it knows
     * what position to generate moves for
     */
    MoveGen *generator;

    /*
     * a class to traverse the game tree and figure out the optimal move for the position.
     * this search class is given a pointer to the generator so it can generate moves
     */
    Search *search;

    /*
     * a Piece struct purely used for dragging and rendering pieces. basically a "sprite" struct
     *
     * when the user clicks to move to a square or releases a dragging piece over a square, a move is made.
     * if that move is a promotion, we need to ask the user what piece he wants to promote to. So when we promote a pawn,
     * we need an array of pieces to render for the user to click on to make that choice.
     */
    struct Piece
    {
        SDL_Rect *bounds;
        SDL_Texture *texture;
        PieceType type;
    } promotionChoices[4];

    // nonzero when the program should be "frozen" while we are making our promotion choice
    // holds the square we want to promote on
    uint64_t promotionSquare;

    /*
     * a length 64 array of Square structs is used to represent squares on the screen
     */
    struct Square
    {
        uint8_t squareIndex;
        SDL_Rect *bounds;
        Piece *piece;
        bool isCurrentMove; // highlight move options
        bool isPreviousMove; // highlight where the last player went
        bool isCheckingOrChecked; // highlight the pieces giving check and the checked king
        bool isLight;
    } boardGui[64];

    // so the user or the program can draw nice thick lines on the board
    struct Arrow
    {
        Square *from;
        Square *to;
    };

    // a vector of arrows. an arrow has a start square and an end square
    std::vector<Arrow*> arrows;
    // the arrow the user is choosing an endpoint for
    Arrow *drawing;

    // make the boardGui pieces match the bitboards in the position struct
    void updateBoardGui();

    /*
     * during castling, the king moves first and then the gui is updated with the rook move
     *
     * the player can move a piece by clicking on it and moving it to a highlighted destination square.
     * both the engine and the player can move a piece. it is an animation
     *
     */
    Piece *pieceMoving;
    Square *pieceMovingTarget;
    int pieceMovingXVel;
    int pieceMovingYVel;

    /*
     * when the user drags and drops a piece on the same square it came from,
     * that means the user wants to click and move this piece
     * so this square is the square that is selected by the player
     * it is not being moved or dragged, but it is the last square clicked by the player
     */
    Square *selectedSquare;

    /*
     * only the player can drag a piece.
     */
    Piece *pieceDragging;

    void makePlayerMove(uint8_t from, uint8_t to, Board::MoveType type);
    void makeEngineMove();

    void resetMoveOptions();
    void resetMoveHighlights();

    Square* getSquareClicked(int mouseX, int mouseY);

    // set the moving piece and give it velocity
    void setMovingPiece(Square *from, Square *to);

    void renderSquare(Square &square);
    void renderPiece(Piece *piece);
    void renderArrow(Arrow *arrow);

    SDL_Texture* loadPieceTexture(PieceType type);

};


#endif //UNTITLED2_CHESSGAME_H
