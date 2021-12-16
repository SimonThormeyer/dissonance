/*
 * @author: georgbuechner
 */
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/error.hpp>
#include <websocketpp/frame.hpp>
#include <spdlog/spdlog.h>

#include "nlohmann/json_fwd.hpp"
#include "server/server_game.h"
#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "websocket_server.h"
#include "nlohmann/json.hpp"

#include "utils/utils.h"
#include "websocketpp/close.hpp"
#include "websocketpp/common/system_error.hpp"

#include "constants/codes.h"
#include "websocketpp/logger/levels.hpp"

#define LOGGER "logger"

#ifdef _COMPILE_FOR_SERVER_
namespace asio = websocketpp::lib::asio;
#endif

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

//Function only needed for callback, but nether actually used.
inline std::string get_password() { return ""; }

WebsocketServer::WebsocketServer(bool standalone) : standalone_(standalone) {}

WebsocketServer::~WebsocketServer() {
  server_.stop();
}

#ifdef _COMPILE_FOR_SERVER_
context_ptr on_tls_init(websocketpp::connection_hdl hdl) {
  context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>
    (asio::ssl::context::sslv23);
  try {
    ctx->set_options(asio::ssl::context::default_workarounds |
      asio::ssl::context::no_sslv2 |
      asio::ssl::context::no_sslv3 |
      asio::ssl::context::no_tlsv1 |
      asio::ssl::context::single_dh_use);
    ctx->set_password_callback(bind(&get_password));
    ctx->use_certificate_chain_file("/etc/letsencrypt/live/kava-i.de-0001/fullchain.pem");
    ctx->use_private_key_file("/etc/letsencrypt/live/kava-i.de-0001/privkey.pem", 
        asio::ssl::context::pem);

    std::string ciphers;
    ciphers = "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-"
      "AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:"
      "ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:"
      "ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-"
      "RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA384:ECDHE-ECDSA-"
      "AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-"
      "RSA-AES256-SHA256:DHE-RSA-AES256-SHA:ECDHE-ECDSA-DES-CBC3-SHA:ECDHE-RSA-DES-CBC3-"
      "SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-"
      "SHA256:AES128-SHA:AES256-SHA:DES-CBC3-SHA:!DSS";
    if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1) {
      spdlog::get(LOGGER)->error("WebsocketFrame::on_tls_init: error setting cipher list");
    }
  } catch (std::exception& e) {
    spdlog::get(LOGGER)->error("WebsocketFrame::on_tls_init: {}", e.what());
  }
  return ctx;
}
#endif


void WebsocketServer::Start(int port) {
  try { 
    spdlog::get(LOGGER)->info("WebsocketFrame::Start: set_access_channels");
    server_.set_access_channels(websocketpp::log::alevel::none);
    server_.clear_access_channels(websocketpp::log::alevel::frame_payload);
    server_.init_asio(); 
    server_.set_message_handler(bind(&WebsocketServer::on_message, this, &server_, ::_1, ::_2)); 
    #ifdef _COMPILE_FOR_SERVER_
      server_.set_tls_init_handler(bind(&on_tls_init, ::_1));
    #endif
    server_.set_open_handler(bind(&WebsocketServer::OnOpen, this, ::_1));
    server_.set_close_handler(bind(&WebsocketServer::OnClose, this, ::_1));
    server_.set_reuse_addr(true);
    server_.listen(port); 
    server_.start_accept(); 
    spdlog::get(LOGGER)->info("WebsocketFrame:: successfully started websocket server on port: {}", port);
    server_.run(); 
  } catch (websocketpp::exception const& e) {
    std::cout << "WebsocketFrame::Start: websocketpp::exception: " << e.what() << std::endl;
  } catch (std::exception& e) {
    std::cout << "WebsocketFrame::Start: websocketpp::exception: " << e.what() << std::endl;
  }
}

void WebsocketServer::OnOpen(websocketpp::connection_hdl hdl) {
  std::unique_lock ul(shared_mutex_connections_);
  if (connections_.count(hdl.lock().get()) == 0) {
    // spdlog::get(LOGGER)->info("WebsocketFrame::OnOpen: New connectionen added.");
    connections_[hdl.lock().get()] = new Connection(hdl, hdl.lock().get(), "");
  }
  else
    spdlog::get(LOGGER)->warn("WebsocketFrame::OnOpen: New connection, but connection already exists!");
}

void WebsocketServer::OnClose(websocketpp::connection_hdl hdl) {
  std::unique_lock ul(shared_mutex_connections_);
  if (connections_.count(hdl.lock().get()) > 0) {
    try {
      delete connections_[hdl.lock().get()];
      if (connections_.size() > 1)
        connections_.erase(hdl.lock().get());
      else 
        connections_.clear();
      spdlog::get(LOGGER)->info("WebsocketFrame::OnClose: Connection closed successfully.");
      spdlog::get(LOGGER)->info("WebsocketFrame::OnClose: running games {}, active connections {}, in_mappings: {}",
          games_.size(), connections_.size(), username_game_id_mapping_.size());
    } catch (std::exception& e) {
      spdlog::get(LOGGER)->error("WebsocketFrame::OnClose: Failed to close connection: {}", e.what());
    }
  }
  else 
    spdlog::get(LOGGER)->info("WebsocketFrame::OnClose: Connection closed, but connection didn't exist!");
}

void WebsocketServer::on_message(server* srv, websocketpp::connection_hdl hdl, message_ptr msg) {
  // Validate json.
  spdlog::get(LOGGER)->info("Websocket::on_message: validating new msg: {}", msg->get_payload());
  auto json_opt = utils::ValidateJson({"command", "username", "data"}, msg->get_payload());
  if (!json_opt.first) {
    spdlog::get(LOGGER)->warn("Server: message with missing required fields (required: command, username, data)");
    return;
  }
  std::string command = json_opt.second["command"];
  std::string username = json_opt.second["username"];
  
  // Create controller connection
  if (command == "initialize")
    h_InitializeUser(hdl.lock().get(), username, json_opt.second);
  // Start new game
  else if (command == "init_game")
    h_InitializeGame(hdl.lock().get(), username, json_opt.second);
  else if (command == "close")
    h_CloseGame(hdl.lock().get(), username, json_opt.second);
  // In game action
  else
    h_InGameAction(hdl.lock().get(), username, json_opt.second);
}

void WebsocketServer::h_InitializeUser(connection_id id, std::string username, nlohmann::json& msg) {
  // Create controller connection.
  std::unique_lock ul(shared_mutex_connections_);
  if (connections_.count(id)) {
    connections_[id]->set_username(username);
    ul.unlock();
    nlohmann::json resp = {{"command", "select_mode"}};
    SendMessage(id, resp.dump());
  }
  else {
    spdlog::get(LOGGER)->warn("WebsocketServer::h_InitializeUser: connection does not exist! {}", username);
  }
}

void WebsocketServer::h_InitializeGame(connection_id id, std::string username, nlohmann::json& msg) {
  nlohmann::json data = msg["data"];
  if (data["mode"] == SINGLE_PLAYER) {
    std::unique_lock ul(shared_mutex_games_);
    spdlog::get(LOGGER)->info("Server: initializing new single-player game.");
    std::string game_id = username;
    username_game_id_mapping_[username] = game_id;
    games_[game_id] = new ServerGame(data["lines"], data["cols"], data["mode"], data["base_path"],
        this, game_id, username);
    nlohmann::json resp = {{"command", "select_audio"}, {"data", nlohmann::json()}};
    SendMessage(id, resp.dump());
  }
  else {
    spdlog::get(LOGGER)->warn("Server: unkown game mode (0: single, 1: multi, 2: observer)");
  }
}

void WebsocketServer::h_InGameAction(connection_id id, std::string username, nlohmann::json& msg) {
  std::shared_lock sl(shared_mutex_games_);
  if (username_game_id_mapping_.count(username) == 0)
    return;
  std::string game_id = username_game_id_mapping_.at(username);
  if (games_.count(game_id)) {
    nlohmann::json resp = games_.at(username)->HandleInput(msg["command"], msg);
    if (resp.contains("command"))
      SendMessage(id, resp.dump());
  }
  else {
    spdlog::get(LOGGER)->warn("Server: message with unkown command or username.");
  }
}

void WebsocketServer::h_CloseGame(connection_id id, std::string username, nlohmann::json& msg) {
  std::unique_lock ul_connections(shared_mutex_connections_);
  connections_.at(id)->set_closed(true);

  std::shared_lock sl(shared_mutex_games_);
  if (username_game_id_mapping_.count(username) == 0)
    return;
  std::string game_id = username_game_id_mapping_.at(username);
  std::vector<std::string> all_users = games_.at(game_id)->GetPlayingUsers();
  bool all_clients_closed = true;
  for (const auto& it : all_users) {
    if (!connections_.at(GetConnectionIdByUsername(it))->closed())
      all_clients_closed = false;
  }
  spdlog::get(LOGGER)->info("All clients closed: {}", all_clients_closed);
  spdlog::get(LOGGER)->flush();

  if (all_clients_closed) {
    while(true) {
      spdlog::get(LOGGER)->info("Game status {}", games_.at(game_id)->status());
      spdlog::get(LOGGER)->flush();

      if (games_.at(game_id)->status() == CLOSED) {
        // Delete game and remove from list of games.
        delete games_[game_id];
        games_.erase(game_id);
        std::string all_user_str;
        for (const auto& it : all_users) 
          all_user_str += it + ", ";
        spdlog::get(LOGGER)->info("Deleted game with id {} for users {}", game_id, all_user_str);
        spdlog::get(LOGGER)->flush();
        // Remove all username-game-mappings
        for (const auto& it : all_users)
          username_game_id_mapping_.erase(it);
        spdlog::get(LOGGER)->info("Removed all {} user mappings", all_users.size());
        spdlog::get(LOGGER)->flush();
        break;
      }
    }
  }
  if (!standalone_)
    exit(0);
}

WebsocketServer::connection_id WebsocketServer::GetConnectionIdByUsername(std::string username) {
  spdlog::get(LOGGER)->info("WebsocketServer::GetConnectionIdByUsername: username: {}", username);
  for (const auto& it : connections_)
    if (it.second->username() == username) 
      return it.second->get_connection_id();
  return connection_id();
}

void WebsocketServer::SendMessage(connection_id id, std::string msg) {
  try {
    // Set last incomming and get connection hdl from connection.
    std::shared_lock sl(shared_mutex_connections_);
    if (connections_.count(id) == 0) {
      spdlog::get(LOGGER)->error("WebsocketFrame::SendMessage: failed to get connection to {}", id);
      return;
    }
    spdlog::get(LOGGER)->info("WebsocketFrame::SendMessage: sending message to {} - {}: {}", 
        id, connections_.at(id)->username(), msg);
    websocketpp::connection_hdl hdl = connections_.at(id)->connection();
    sl.unlock();

    // Send message.
    server_.send(hdl, msg, websocketpp::frame::opcode::value::text);
    spdlog::get(LOGGER)->debug("WebsocketFrame::SendMessage: successfully sent message.");
  } catch (websocketpp::exception& e) {
    spdlog::get(LOGGER)->error("WebsocketFrame::SendMessage: failed sending message: {}", e.what());
  } catch (std::exception& e) {
    spdlog::get(LOGGER)->error("WebsocketFrame::SendMessage: failed sending message: {}", e.what());
  }
}