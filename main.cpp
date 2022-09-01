//
// Created by Joe Chrisman on 2/23/22.
//

#include "ChessGame.h"
#include "iostream"

SDL_Window *window;
SDL_Renderer *renderer;
ChessGame *game;

void start()
{
    srand(time(nullptr));
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(WINDOW_SIZE, WINDOW_SIZE, 0, &window, &renderer);
    game = new ChessGame(renderer);
}

void run()
{
    game->render();
    uint32_t lastFrameTicks = SDL_GetTicks();

    while (true)
    {
        uint32_t ticks = SDL_GetTicks();
        if (ticks - lastFrameTicks > 1000 / FRAMERATE)
        {
            lastFrameTicks = ticks;

            // don't rerender the frame if nothing changed
            bool shouldRenderFrame = game->isAnimating();
            game->updateMovingPiece();

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                // if user clicked the red x button
                if (event.type == SDL_QUIT)
                {
                    // quit the program
                    return;
                }

                if (event.type == SDL_MOUSEBUTTONUP)
                {
                    game->onMouseReleased(event.button.x, event.button.y);
                    shouldRenderFrame = true;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN)
                {
                    game->onMousePressed(event.button.x, event.button.y, event.button.button == SDL_BUTTON_RIGHT);
                    shouldRenderFrame = true;
                }
                else if (event.type == SDL_MOUSEMOTION)
                {
                    game->onMouseMoved(event.button.x, event.button.y);
                    shouldRenderFrame = true;
                }
            }

            if (game->isAnimating() || shouldRenderFrame)
            {
                game->render();
            }
        }
    }
}

void stop()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main()
{
    start();
    run();
    stop();

    return 0;
}
