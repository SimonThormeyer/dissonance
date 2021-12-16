#ifndef SRC_SERVER_GAME_H_
#define SRC_SERVER_GAME_H_

#include "nlohmann/json_fwd.hpp"
#include "share/eventmanager.h"
#define NCURSES_NOMACROS
#include <cstddef>
#include <curses.h>
#include <mutex>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <shared_mutex>
#include <vector>

#include "audio/audio.h"
#include "constants/texts.h"
#include "game/field.h"
#include "player/audio_ki.h"
#include "player/player.h"
#include "objects/units.h"
#include "random/random.h"

class WebsocketServer;

class ServerGame {
  public:
    /** 
     * Constructor initializing game with availible lines and columns.
     * @param[in] lines availible lines.
     * @param[in] cols availible cols
     */
    ServerGame(int lines, int cols, int mode, std::string base_path, WebsocketServer* srv, std::string game_id,
        std::string usr1, std::string usr2="");

    // getter 
    int status();

    // setter 
    void set_status(int status);

    std::vector<std::string> GetPlayingUsers();

    /**
     * Handls input
     */
    nlohmann::json HandleInput(std::string command, nlohmann::json msg);
    
    // Threads
    void Thread_RenderField();
    void Thread_Ai();

  private: 
    Field* field_;
    Player* player_one_;
    Player* player_two_;
    bool game_over_;
    bool pause_;
    bool resigned_;
    Audio audio_;
    WebsocketServer* ws_server_;
    EventManager<std::string, ServerGame, nlohmann::json&> eventmanager_;
    const std::string game_id_;
    const std::string usr1_id_;
    const std::string usr2_id_;

    std::shared_mutex mutex_status_;  ///< mutex locked, when printing field.
    int status_;

    const int mode_;
    const int lines_;
    const int cols_;

    // handlers
    nlohmann::json InitializeGame(nlohmann::json data);

    // command methods

    void m_AnalyzeAudio(nlohmann::json&);
    void m_AddIron(nlohmann::json&);
    void m_RemoveIron(nlohmann::json&);
    void m_AddTechnology(nlohmann::json&);
    void m_Resign(nlohmann::json&);
};

#endif