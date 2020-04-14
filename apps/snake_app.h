// Copyright (c) 2020 CS126SP20. All rights reserved.

#ifndef SNAKE_SNAKEAPP_H_
#define SNAKE_SNAKEAPP_H_

#include <cinder/app/App.h>
#include <cinder/gl/gl.h>
#include <snake/engine.h>
#include <snake/leaderboard.h>
#include <snake/player.h>

#include <random>
#include <string>
#include <vector>

using std::chrono::seconds;

namespace snakeapp {

enum class GameState {
  kPlaying,
  kCountDown,
  kGameOver,
};

class SnakeApp : public cinder::app::App {
 public:
  SnakeApp();
  void setup() override;
  void update() override;
  void draw() override;
  void keyDown(cinder::app::KeyEvent) override;
  // Allow the user to move the snake with the mouse instead
  void mouseDown(cinder::app::MouseEvent) override;
  void mouseWheel(cinder::app::MouseEvent) override;

 private:
  void DrawBackground() const;
  void DrawCountDown() const;
  void DrawFood() const;
  void DrawGameOver();
  void DrawSnake() const;
  float PercentageOver() const;
  void ResetGame();

 private:
  snake::Engine engine_;
  std::chrono::time_point<std::chrono::system_clock> last_intact_time_;
  std::chrono::time_point<std::chrono::system_clock> last_pause_time_;
  std::chrono::time_point<std::chrono::system_clock> last_time_;
  snake::LeaderBoard leaderboard_;
  bool paused_;
  const std::string player_name_;
  bool printed_game_over_;
  const size_t size_;
  const size_t speed_;
  GameState state_;
  const size_t tile_size_;
  size_t time_left_;
  std::vector<snake::Player> top_players_;

  // Vector for the top scores of the user
  std::vector<snake::Player> your_top_scores_;
  // Amount of time before the color of the food should be changed
  double color_change_time_;
  // Following are the three indexes for color
  double color_index_one_;
  double color_index_two_;
  double color_index_three_;
  // Keeps track of when the color_change_time_ has started
  std::chrono::time_point<std::chrono::system_clock> last_color_time_;
  // Track off snake size
  size_t snake_size_;
  seconds time_played_;
  // Snake colors
  double snake_index_one_;
  double snake_index_two_;
  double snake_index_three_;
};

}  // namespace snakeapp

#endif  // SNAKE_SNAKEAPP_H_
