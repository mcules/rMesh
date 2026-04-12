#pragma once

#include "IBoardConfig.h"

/**
 * @file BoardFactory.h
 * @brief Factory that returns the IBoardConfig for the compiled target board.
 *
 * Usage (once in main.cpp):
 *   IBoardConfig* board = BoardFactory::create();
 */
class BoardFactory {
public:
    /**
     * @brief Returns a heap-allocated IBoardConfig for the active board.
     *
     * The returned pointer is valid for the lifetime of the application.
     * BoardFactory.cpp is the ONLY file in the project that contains
     * #ifdef BOARD_XYZ checks.
     */
    static IBoardConfig* create();
};
