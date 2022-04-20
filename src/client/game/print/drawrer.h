#ifndef SRC_PRINT_DRAWER_H_
#define SRC_PRINT_DRAWER_H_

#include "server/game/player/statistics.h"
#include "share/objects/lobby.h"
#include "spdlog/spdlog.h"
#include <set>
#include <vector>
#define NCURSES_NOMACROS
#include <cstddef>
#include <curses.h>
#include <shared_mutex>
#include <string>

#include "nlohmann/json.hpp"

#include "share/constants/codes.h"
#include "share/constants/costs.h"
#include "share/constants/texts.h"
#include "share/objects/transfer.h"
#include "share/tools/utils/utils.h"
#include "share/defines.h"

class Drawrer {
  public:
    Drawrer();

    void SetUpBorders(int lines, int cols);

    // getter
    int field_height();
    int field_width();
    position_t field_pos();
    std::string game_id_from_lobby();

    int GetResource(); 
    int GetTech(); 

    // setter 
    void set_viewpoint(int viewpoint);
    void inc_cur_sidebar_elem(int value); 
    void set_msg(std::string msg);
    void set_transfer(nlohmann::json& data);
    void set_stop_render(bool stop);
    void set_field_start_pos(position_t pos);
    void set_range(std::pair<position_t, int> range);
    void set_topline(t_topline topline);
    void set_mode(int mode);
    void set_statistics(nlohmann::json json);

    bool InGraph(position_t);
    bool Free(position_t);
    void AddMarker(int type, position_t pos, int color, std::string symbol = "");
    position_t GetMarkerPos(int type, std::string symbol);
    void ClearMarkers(int type = -1);

    /**
     * Adds a new neuron to given position.
     * @param[in] pos
     * @param[in] unit
     */
    void AddNewUnitToPos(position_t pos, int unit, int color);

    /**
     * Updates transfer object with new infos.
     * @param[in] transfer_json
     */
    void UpdateTranser(nlohmann::json& transfer_json);

    /**
     * Updates lobby (Locks mutex: unique)
     * param[in] lobby_json
     */
    void UpdateLobby(nlohmann::json& lobby_json, bool init=false);

    /**
     * Move viewpoint to next viewpoint
     * @return current context.
     */
    int next_viewpoint();

    /**
     * Clears and refreshes field, locks mutex.
     */
    void ClearField();

    /**
     * Prints a single line centered. (Does not clear screen, does not lock mutex)
     * @param[in] l (what line to print to)
     * @param[in] line (text to print
     */
    void PrintCenteredLine(int l, std::string line) const;
    void PrintCenteredLineBold(int l, std::string line) const;

    void PrintStatistics() const;

    /**
     * Prints a single line centered, but here the line is splitted into parts,
     * with each part assined it's own color. (Does not clear screen, does not lock mutex)
     * @param[in] l line to print to
     * @param[in] txt_with_color array of text parts, each with a color.
     */
    void PrintCenteredLineColored(int line, std::vector<std::pair<std::string, int>> txt_with_color);

    /**
     * Prints a paragraph (mulitple lines of text) to the center of the screen.
     * (Does not clear screen, does not lock mutex)
     * @param[in] paragraph
     * @param[in] nextmsg adds a message with instructions for prev/next/quit (default=false)
     */
    void PrintCenteredParagraph(texts::paragraph_t paragraph, bool nextmsg=false);
    void PrintCenteredParagraph(texts::paragraph_t paragraph, unsigned int start_height);
    void PrintCenteredParagraphAndMiniFields(texts::paragraph_t paragraph, std::vector<std::string> field, 
        bool nextmsg=false);

    /**
     * Prints mulitple paragraphs to the center of the screen, clearing screen
     * before every new paragraph and locking mutex.
     * @param[in] paragraphs
     */
    void PrintCenteredParagraphs(texts::paragraphs_t paragraph, bool skip_first_wait=false);

    void PrintGame(bool only_field, bool only_side_column, int context);

    /** 
     * Prints lobby (Locks mutex: unique)
     */
    void PrintLobby();

    void CreateMiniFields(int player_color);

  private:
    int lines_;
    int cols_;
    int mode_;
    Transfer transfer_;
    Lobby lobby_;
    std::vector<std::vector<Transfer::Symbol>> field_;
    std::set<position_t> graph_positions_;
    std::map<position_t, Transfer::Symbol> temp_symbols_;
    std::map<int, std::map<position_t, std::pair<std::string, int>>> markers_;
    t_topline topline_;
    std::shared_mutex mutex_print_field_;  ///< mutex locked, when printing field.
    bool stop_render_;
    std::map<std::string, Statictics> statistics_;
    std::map<std::string, std::vector<std::vector<Transfer::Symbol>>> mini_fields_;

    // Selection
    struct ViewPoint {
      public: 
        ViewPoint() {};
        ViewPoint(int x, int y, void(ViewPoint::*f_inc)(int val), std::string(ViewPoint::*f_ts)(const Transfer&)) 
          : x_(x), y_(y), inc_(f_inc), to_string_(f_ts) {
            spdlog::get(LOGGER)->info("Created new ViewPoint with x={} and y={}", x_, y_);
          }

        // member
        int x_;
        int y_;
        std::pair<position_t, int> range_; // start, range
        std::set<position_t> graph_positions_;

        // methods
        void inc(int val) { (this->*inc_)(val); }
        std::string to_string(const Transfer& transfer) { return(this->*to_string_)(transfer); }

        void inc_resource(int val) { x_= utils::Mod(x_+val, SEROTONIN+1); }
        void inc_tech(int val) { x_= utils::Mod(x_+val, NUCLEUS_RANGE+1, WAY); }
        void inc_stats(int val) { x_ = utils::Mod(x_+val, y_); }
        void inc_field(int val) { 
          spdlog::get(LOGGER)->debug("ViewPoint::inc_field: Changing field selection: {}|{}", y_, x_);
          int old_x = x_;
          int old_y = y_;

          position_t next_diag;
          if (val == 1) {
            y_++;
            next_diag = {y_, x_+1};
          }
          else if (val == -1) {
            y_--;
            next_diag = {y_, x_-1};
          }
          else if (val == 2) {
            x_++;
            next_diag = {y_-1, x_};
          }
          else if (val == -2) {
            x_--;
            next_diag = {y_+1, x_};
          }

          position_t pos = {y_, x_};
          // If not full field range: check if next position is a) still in range and b) valid graph positions
          if (range_.second < 999 && 
              (utils::Dist(range_.first, pos) > range_.second || graph_positions_.count(pos) == 0)) {
            // If diagonal position possible, do that.
            if (graph_positions_.count(next_diag) > 0 && utils::Dist(range_.first, next_diag) <= range_.second) {
              y_ = next_diag.first;
              x_ = next_diag.second;
            }
            // Otherwise, revert changes.
            else {
              x_ = old_x;
              y_ = old_y;
            }
          }
        }

        std::string to_string_resource(const Transfer& transfer) {   
          Transfer::Resource resource = transfer.resources().at(x_);
          return resources_name_mapping.at(x_) + ": " + resource.value_ + "+" 
            + resource.bound_ + "/" + resource.limit_ + " ++" + resource.iron_
            + "$" + resource_description_mapping.at(x_);
        }

        std::string to_string_tech(const Transfer& transfer) {
          Transfer::Technology tech = transfer.technologies().at(x_);
          std::string costs = "costs: ";
          for (const auto& it : costs::units_costs_.at(x_)) {
            if (it.second > 0)
              costs += resources_name_mapping.at(it.first) + ": " + utils::Dtos(it.second) + ", ";
          }
          return units_tech_name_mapping.at(x_) + ": " + tech.cur_ + "/" + tech.max_ 
            + "$" + units_tech_description_mapping.at(x_) + "$" + costs;
        }

        std::string to_string_field(const Transfer& transfer) {
          return std::to_string(x_) + "|" + std::to_string(y_);
        }

      private:
        void(ViewPoint::*inc_)(int val);
        std::string(ViewPoint::*to_string_)(const Transfer&);
    };
    std::map<int, ViewPoint> cur_selection_; // 0: map, 1: resource, 2: tech
    int cur_view_point_;

    // Boxes
    std::string msg_;

    // start-lines header
    int l_headline_;  ///< start of headline showing most essential game infos
    int l_audio_;  ///< start of audio information
    int l_bar_;  ///< start of help-bar
    int l_main_;  ///< start of main-content
    int l_message_;  ///< start of quick-message
    int l_bottom_;  ///< start of footer
    int extra_height_;  ///< modifies start of field y-coordinate, if size is smaller that screen resolution suggests.
    int extra_width_;  ///< modifies start of field x-coordinate, if size is smaller that screen resolution suggests.

    // start-cols 
    int c_tech_;  ///< start technologies
    int c_field_;  ///< start field
    int c_resources_;  ///< start resources

    // Print methods
    void PrintField();
    void PrintHeader(float audio_played, const t_topline& players);
    void PrintTopline(std::vector<bool> availible_options);
    void PrintTechnologies(const std::string& players);
    void PrintSideColumn(const std::map<int, Transfer::Resource>& resources, 
        const std::map<int, Transfer::Technology>& technologies);
    void PrintMessage();
    void PrintFooter(std::string str);
    void ClearLine(int line, int start_col=0);

    // helpers
    std::pair<std::string, int> GetReplaceMentFromMarker(position_t pos);
};

#endif