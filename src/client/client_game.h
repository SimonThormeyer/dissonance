#ifndef SRC_CLIENT_CLIENT_GAME_H_
#define SRC_CLIENT_CLIENT_GAME_H_

#include "nlohmann/json_fwd.hpp"
#include "print/drawrer.h"
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

class ClientGame {
  public:

    /** 
     * Constructor initializing basic settings and ncurses.
     * @param[in] relative_size
     * @param[in] audio_base_path
     */
    ClientGame(bool relative_size, std::string base_path, std::string username);

    nlohmann::json HandleAction(nlohmann::json);

    void GetAction();

  private: 
    // member variables.
    const std::string username_;
    int lines_;
    int cols_;
    const std::string base_path_;
    std::shared_mutex mutex_print_;  ///< mutex locked, when printing.
    bool render_pause_;
    Drawrer drawrer_;
    bool action_;

    std::vector<std::string> audio_paths_; 

    /**
     * Shows player main-menu: with basic game info and lets player pic singe/
     * muliplayer
     */
    nlohmann::json Welcome();


    // Selection methods
    
    std::pair<std::string, nlohmann::json> DistributeIron(nlohmann::json resources);
    
    /**
     * Selects one of x options. (Clears field and locks mutex; pauses game??)
     * @param[in] instruction to let the user know what to do
     * @param[in] force_selection indicating whether tp force user to select)
     * @param[in] mapping which maps a int to a string.
     * @param[in] splits indicating where to split options.
     */
    int SelectInteger(std::string instruction, bool force_selection, choice_mapping_t& mapping, 
        std::vector<size_t> splits);

    struct AudioSelector {
      std::string path_;
      std::string title_;
      std::vector<std::pair<std::string, std::string>> options_;
    };

    AudioSelector SetupAudioSelector(std::string path, std::string title, std::vector<std::string> paths);

    /**
     * Select path to audio-file (Clears field).
     * @return path to audio file.
     */
    std::string SelectAudio();

    // input methods

    /**
     * Input simple string (Clears field)
     * @param[in] instruction to let the user know what to do
     * @return user input (string)
     */
    std::string InputString(std::string msg);
};

#endif
