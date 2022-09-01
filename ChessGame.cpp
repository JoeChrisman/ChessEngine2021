//
// Created by Joe Chrisman on 2/23/22.
//

#include "ChessGame.h"

ChessGame::ChessGame(SDL_Renderer *renderer)
{
    this->renderer = renderer;

    // zero initialize everything
    pieceDragging = nullptr;
    pieceMoving = nullptr;
    pieceMovingTarget = nullptr;
    pieceMovingXVel = 0;
    pieceMovingYVel = 0;

    promotionSquare = 0;

    selectedSquare = nullptr;
    drawing = nullptr;

    // initialize the boardGui Square array
    for (uint8_t square = 0; square < 64; square++)
    {
        PieceType type = INITIAL_BOARD[square];
        int row = square / 8;
        int col = square % 8;
        SDL_Rect *squareBounds = new SDL_Rect{col * SQUARE_SIZE, row * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE};

        // get a pointer to the piece on this square
        Piece *piece = type == NONE ? nullptr : new Piece{
            new SDL_Rect{squareBounds->x, squareBounds->y, squareBounds->w, squareBounds->h},
            loadPieceTexture(type),
            type
        };

        // initialize the square in the boardGui
        boardGui[square] = Square{
                square,
                squareBounds,
                piece,
                false,
                false,
                false,
                row % 2 == col % 2,
        };
    }

    // initialize the array of pieces we click on when we make a promotion choice.
    // a lot of the time, we don't care about these. we only use them when we display the promotion options
    promotionChoices[0] = Piece{
                    new SDL_Rect{SQUARE_SIZE * 2, SQUARE_SIZE * 3, SQUARE_SIZE, SQUARE_SIZE},
                    loadPieceTexture(PLAYER_QUEEN),
                    PLAYER_QUEEN
    };
    promotionChoices[1] = Piece{
            new SDL_Rect{SQUARE_SIZE * 3, SQUARE_SIZE * 3, SQUARE_SIZE, SQUARE_SIZE},
            loadPieceTexture(PLAYER_ROOK),
            PLAYER_ROOK
    };

    promotionChoices[2] = Piece{
            new SDL_Rect{SQUARE_SIZE * 4, SQUARE_SIZE * 3, SQUARE_SIZE, SQUARE_SIZE},
            loadPieceTexture(PLAYER_BISHOP),
            PLAYER_BISHOP
    };

    promotionChoices[3] = Piece{
            new SDL_Rect{SQUARE_SIZE * 5, SQUARE_SIZE * 3, SQUARE_SIZE, SQUARE_SIZE},
            loadPieceTexture(PLAYER_KNIGHT),
            PLAYER_KNIGHT
    };

    board = new Board();
    generator = new MoveGen(board);
    search = new Search(generator);

    if (ENGINE_IS_WHITE)
    {
        makeEngineMove();
    }
}


/*
 * synchronise boardGui data with board.position data (gets called when a move is executed)
 * the reason we are keeping gui separate from board representation is because
 * we need to keep piece images loaded in between frames. we have to support dragging
 * and allow a framerate that is not hilariously slow. this way we don't have
 * to call SDL_LoadBMP and recalculate all the piece images every frame
 *
 * this function also re-renders!
 */
void ChessGame::updateBoardGui()
{
    // go through all the squares on the board
    for (uint8_t square = 0; square < 64; square++)
    {
        boardGui[square].piece = nullptr;

        // if there is a piece on this square in the bitboard data
        PieceType type = board->getPieceType(square);
        if (type != NONE)
        {
            int row = square / 8;
            int col = square % 8;
            SDL_Rect *pieceBounds = new SDL_Rect{col * SQUARE_SIZE, row * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE};
            // create a piece on this square
            boardGui[square].piece = new Piece{pieceBounds, loadPieceTexture(type)};
        }
    }

    render();
}

/*
 * make a move for the player on the board. this includes promotions, en passant, all types of moves
 * it changes data in the position struct of the board object, but is not used during the search.
 * this is the function that gets called when the user wants to move a piece.
 *
 * the purpose of this function is to figure out the proper move type given a square to and from, and
 * then to execute the move on the position struct in the board class
 */
void ChessGame::makePlayerMove(uint8_t from, uint8_t to, Board::MoveType type)
{
    resetMoveOptions();
    resetMoveHighlights();

    // if we think we are making a normal move
    if (type == Board::NORMAL)
    {
        // if we are moving a pawn - we might be promoting or doing en passant. we need to change the type for these special moves
        if (boardOf(from) & (board->position.pieces[PLAYER_PAWN]))
        {
            // if we are actually promoting a pawn
            if (boardOf(to) & RANK0)
            {
                // give the user a choice of what piece type he wants to promote to.
                // there are 4 choices: queen, rook, bishop, or knight.
                // also, we need to remember what square we want to promote on
                promotionSquare = boardOf(to);
                return;
            }

            // if we are actually capturing a pawn en passant
            else if ((from % 8 != to % 8) && (board->getPieceType(to) == NONE))
            {
                type = Board::EN_PASSANT;
            }
        }
    }

    Board::Move move{
            type,
            from,
            to,
            board->getPieceType(from),
            board->getPieceType(to)
    };

    if (type == Board::EN_PASSANT)
    {
        move.captured = board->engineToMove ? PLAYER_PAWN : ENGINE_PAWN;
    }

    board->makeMove<false>(move);

    // we may have not displayed this move on the screen yet. the piece may still be moving.
    // highlight the squares we moved to and from to make it extra clear to the user
    boardGui[from].isPreviousMove = true;
    boardGui[to].isPreviousMove = true;
}

// set the moving piece and give it a heading
void ChessGame::setMovingPiece(Square *from, Square *to)
{
    selectedSquare = from;

    pieceMoving = from->piece;
    pieceMovingTarget = to;
    pieceMovingXVel = (to->bounds->x - pieceMoving->bounds->x) / ANIMATION_SPEED;
    pieceMovingYVel = (to->bounds->y - pieceMoving->bounds->y) / ANIMATION_SPEED;

    resetMoveOptions();
    resetMoveHighlights();
    from->isPreviousMove = true;
    to->isPreviousMove = true;
}

void ChessGame::updateMovingPiece()
{
    // if the piece is moving
    if (pieceMoving && !promotionSquare)
    {
        assert(pieceMovingTarget);
        assert(pieceMovingXVel || pieceMovingYVel);

        // move it along little by little
        pieceMoving->bounds->x += pieceMovingXVel;
        pieceMoving->bounds->y += pieceMovingYVel;

        SDL_Rect *piece = pieceMoving->bounds;
        SDL_Rect *target = pieceMovingTarget->bounds;

        // if the moving piece has reached its target.
        // this moving piece might be an engine piece, or it might be a player piece.
        if (distance(piece->x, piece->y, target->x, target->y) < distance(0, 0, pieceMovingXVel, pieceMovingYVel))
        {
            // this move is already in the position struct of the board class
            // we put it there when we clicked on the destination square to start moving the piece here
            // so all we have to do it update the gui and display the move
            // but before then, snap the bounds of the moving piece to the destination square
            // and rerender to give a less jittery effect (because updateBoardGui() is slow)
            *(pieceMoving->bounds) = *(pieceMovingTarget->bounds);
            render();

            // stop animating this piece
            pieceMoving = nullptr;
            pieceMovingTarget = nullptr;
            pieceMovingXVel = 0;
            pieceMovingYVel = 0;
            updateBoardGui();

            // if we need to make an engine move, make an engine move
            if (board->engineToMove)
            {
                makeEngineMove();
            }

        }
    }
}

/*
 * render the squares and the pieces on the chess board
 * also render the piece moving or the piece dragging
 */
void ChessGame::render()
{
    SDL_RenderClear(renderer);

    // if we need to decide a piece to promote
    if (promotionSquare)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, nullptr);
        for (Piece &piece : promotionChoices)
        {
            renderPiece(&piece);
        }
    }
    else
    {
        for (Square &square : boardGui)
        {
            renderSquare(square);
        }

        for (Arrow *arrow : arrows)
        {
            renderArrow(arrow);
        }

        // the piece being dragged by the player
        if (pieceDragging)
        {
            renderPiece(pieceDragging);
        }
        // the piece being moved by the player or the engine
        if (pieceMoving)
        {
            renderPiece(pieceMoving);
        }
    }

    SDL_RenderPresent(renderer);
}

void ChessGame::renderSquare(Square &square)
{
    // set colors for a previous move square, checking square, light square, or dark square
    if (square.isPreviousMove)
    {
        SDL_SetRenderDrawColor(renderer, Colors::PREVIOUS_MOVE_R, Colors::PREVIOUS_MOVE_G, Colors::PREVIOUS_MOVE_B, 255);
    }
    else if (square.isCheckingOrChecked)
    {
        SDL_SetRenderDrawColor(renderer, Colors::WARNING_R, Colors::WARNING_G, Colors::WARNING_B, 255);
    }
    else if (square.isLight)
    {
        SDL_SetRenderDrawColor(renderer, Colors::LIGHT_SQUARE_R, Colors::LIGHT_SQUARE_G, Colors::LIGHT_SQUARE_B, 255);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, Colors::DARK_SQUARE_R, Colors::DARK_SQUARE_G, Colors::DARK_SQUARE_B, 255);
    }

    // render the square
    SDL_RenderFillRect(renderer, square.bounds);
    // if the square we are rendering has a piece
    if (square.piece)
    {
        // render the piece
        renderPiece(square.piece);
    }

    // if we are rendering a move option
    if (square.isCurrentMove)
    {
        // set color for move option
        SDL_SetRenderDrawColor(renderer, Colors::MOVE_OPTION_R, Colors::MOVE_OPTION_G, Colors::MOVE_OPTION_B, 255);
        // draw small square inside the square
        SDL_RenderFillRect(renderer,new SDL_Rect{
                square.bounds->x + SQUARE_SIZE * 7 / 16, square.bounds->y + SQUARE_SIZE * 7 / 16, SQUARE_SIZE / 8, SQUARE_SIZE / 8
        });
    }
}

void ChessGame::renderPiece(Piece *piece)
{
    SDL_RenderCopy(renderer, piece->texture, nullptr, piece->bounds);
}

void ChessGame::renderArrow(Arrow *arrow)
{
    SDL_Point destination = SDL_Point{arrow->to->bounds->x + arrow->to->bounds->w / 2, arrow->to->bounds->y + arrow->to->bounds->h / 2};
    SDL_Point origin = SDL_Point{arrow->from->bounds->x + arrow->from->bounds->w / 2, arrow->from->bounds->y + arrow->from->bounds->h / 2};

    SDL_SetRenderDrawColor(renderer, Colors::ARROW_R, Colors::ARROW_G, Colors::ARROW_B, 255);

    // there is no native support in SDL2 to draw a line with a given thickness.
    // so we are going to draw a bunch of lines randomly offset around the original line to make it thicker
    for (int i = 0; i < 500; i++)
    {
        int xOff = 5 - rand() % 10;
        int yOff = 5 - rand() % 10;
        SDL_RenderDrawLine(renderer, origin.x + xOff, origin.y + yOff, destination.x + xOff, destination.y + yOff);
    }
}


void ChessGame::onMouseMoved(int mouseX, int mouseY)
{
    // if we are dragging a piece
    if (pieceDragging)
    {
        // move the piece being dragged to the mouse cursor
        pieceDragging->bounds->x = mouseX - SQUARE_SIZE / 2;
        pieceDragging->bounds->y = mouseY - SQUARE_SIZE / 2;
    }
}


void ChessGame::onMousePressed(int mouseX, int mouseY, bool isRightClick)
{
    // if we clicked while making a promotion choice
    if (promotionSquare && !board->engineToMove)
    {
        SDL_Point *mouse = new SDL_Point{mouseX, mouseY};
        // check to see if we clicked on a promotion option
        for (Piece &choice : promotionChoices)
        {
            // if we made a promotion choice
            if (SDL_PointInRect(mouse, choice.bounds))
            {
                // figure out the correct promotion type
                Board::MoveType promotionType;
                if (choice.type == PLAYER_QUEEN)
                {
                    promotionType = Board::QUEEN_PROMOTION;
                }
                else if (choice.type == PLAYER_ROOK)
                {
                    promotionType = Board::ROOK_PROMOTION;
                }
                else if (choice.type == PLAYER_BISHOP)
                {
                    promotionType = Board::BISHOP_PROMOTION;
                }
                else if (choice.type == PLAYER_KNIGHT)
                {
                    promotionType = Board::KNIGHT_PROMOTION;
                }
                else
                {
                    assert(false);
                }

                // make the move, but with that promotion type
                makePlayerMove(selectedSquare->squareIndex, getLeastSquare(promotionSquare), promotionType);
                promotionSquare = 0;

                // if we dragged the promotion instead of animating it, update to the current board state right away
                if (!pieceMoving)
                {
                    // update to the current board state
                    updateBoardGui();
                    // make the engine move
                    makeEngineMove();
                }
                break;
            }
        }
    }
    else if (!isAnimating())
    {
        // if we clicked a square on the board
        Square *clicked = getSquareClicked(mouseX, mouseY);
        if (clicked)
        {
            // if the user wanted to start creating an arrow
            if (isRightClick)
            {
                drawing = new Arrow{clicked, nullptr};
            }
            // if it is the player's turn
            else if (!board->engineToMove)
            {
                // if we clicked on a square to move our piece there
                if (clicked->isCurrentMove)
                {
                    // make the move in the board, but don't display it yet.
                    // we want to wait until the piece reaches its target until we display the move.
                    makePlayerMove(selectedSquare->squareIndex, clicked->squareIndex, Board::NORMAL);
                    // let the board know we are now animating this piece
                    setMovingPiece(selectedSquare, clicked);

                }
                // if we clicked on a square with our own piece on it
                else if (clicked->piece && clicked->piece->type <= PLAYER_KING)
                {
                    // begin dragging
                    pieceDragging = clicked->piece;
                    selectedSquare = clicked;
                    clicked->piece = nullptr;

                    // figure out the legal moves for the player
                    generator->generatePlayerMoves();
                    // re-highlight legal moves
                    resetMoveOptions();
                    for (Board::Move &move : generator->getSortedMoves())
                    {
                        if (move.from == clicked->squareIndex)
                        {
                            boardGui[move.to].isCurrentMove = true;
                        }
                    }
                }
                // if we clicked anywhere else
                else
                {
                    arrows.clear();
                    resetMoveOptions();
                    selectedSquare = nullptr;
                }
            }
        }
    }
}


void ChessGame::onMouseReleased(int mouseX, int mouseY)
{
    // the square we released the cursor over
    Square *clicked = getSquareClicked(mouseX, mouseY);
    if (clicked)
    {
        // if we are drawing an arrow
        if (drawing)
        {
            drawing->to = clicked;
            if (drawing->to != drawing->from)
            {
                arrows.push_back(drawing);
            }
            drawing = nullptr;
        }
        else if (pieceDragging)
        {
            // if we released the dragging piece over the same square it came from
            if (clicked == selectedSquare)
            {
                // put the piece back where it came from
                *(pieceDragging->bounds) = *(selectedSquare->bounds);
                selectedSquare->piece = pieceDragging;
                pieceDragging = nullptr;
            }
            // if we released the dragging piece over a valid move option square
            else if (clicked->isCurrentMove)
            {
                uint8_t from = selectedSquare->squareIndex;
                uint8_t to = clicked->squareIndex;

                // make the move in the board class
                makePlayerMove(from, to, Board::NORMAL);
                pieceDragging = nullptr;

                // if we don't have to let the user decide a promotion option
                if (!promotionSquare)
                {
                    // the player made a non-promoting move. so make an engine move now
                    updateBoardGui();
                    makeEngineMove();
                }
            }
            else
            {
                // put the piece back where it came from
                *(pieceDragging->bounds) = *(selectedSquare->bounds);
                selectedSquare->piece = pieceDragging;
                pieceDragging = nullptr;
                resetMoveOptions();
            }
        }
    }
}


/*
 * load a .bmp file image in the form of an SDL_Surface object.
 * This SDL_Surface object needs to be freed during deletion by calling SDL_FreeSurface()
 */
SDL_Texture* ChessGame::loadPieceTexture(PieceType type)
{
    // figure out the path to the image for this piece
    char engineColor = ENGINE_IS_WHITE ? 'w' : 'b';
    char playerColor = ENGINE_IS_WHITE ? 'b' : 'w';
    std::string imagePath = "Images/";
    switch(type)
    {
        case NONE: return nullptr;

        case ENGINE_PAWN: imagePath += engineColor; imagePath += 'p'; break;
        case ENGINE_KNIGHT: imagePath += engineColor; imagePath += 'n'; break;
        case ENGINE_BISHOP: imagePath += engineColor; imagePath += 'b'; break;
        case ENGINE_ROOK: imagePath += engineColor; imagePath += 'r'; break;
        case ENGINE_QUEEN: imagePath += engineColor; imagePath += 'q'; break;
        case ENGINE_KING: imagePath += engineColor; imagePath += 'k'; break;

        case PLAYER_PAWN: imagePath += playerColor; imagePath += 'p'; break;
        case PLAYER_KNIGHT: imagePath += playerColor; imagePath += 'n'; break;
        case PLAYER_BISHOP: imagePath += playerColor; imagePath += 'b'; break;
        case PLAYER_ROOK: imagePath += playerColor; imagePath += 'r'; break;
        case PLAYER_QUEEN: imagePath += playerColor; imagePath += 'q'; break;
        case PLAYER_KING: imagePath += playerColor; imagePath += 'k'; break;
    }
    imagePath += ".bmp";
    SDL_Surface *surface = SDL_LoadBMP(imagePath.c_str());
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

ChessGame::Square* ChessGame::getSquareClicked(int mouseX, int mouseY)
{
    // go through the squares on the board
    for (Square &square : boardGui)
    {
        // find the one the user clicked on
        if (SDL_PointInRect(new SDL_Point{mouseX, mouseY}, square.bounds)) {
            return &square;
        }
    }
    return nullptr;
}

void ChessGame::resetMoveOptions()
{
    for (Square &square : boardGui)
    {
        square.isCurrentMove = false;
    }
}

void ChessGame::resetMoveHighlights()
{
    for (Square &square : boardGui)
    {
        square.isPreviousMove = false;
    }
}

void ChessGame::makeEngineMove()
{
    Board::Move best = search->getBestMove();
    setMovingPiece(&boardGui[best.from], &boardGui[best.to]);
    board->makeMove<true>(best);
}

/*
 * we need a way to check if the engine or the player is moving a piece
 * this does not include dragging
 */
bool ChessGame::isAnimating()
{
    return pieceMoving;
}

