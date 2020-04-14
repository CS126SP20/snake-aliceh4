// Copyright (c) 2020 CS126SP20. All rights reserved.

#include "snake_app.h"

#include <cinder/Font.h>
#include <cinder/Text.h>
#include <cinder/Vector.h>
#include <cinder/gl/draw.h>
#include <cinder/gl/gl.h>
#include <gflags/gflags.h>
#include <snake/player.h>
#include <snake/segment.h>
#include <cinder/audio/audio.h>


#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace snakeapp {


using cinder::Color;
using cinder::ColorA;
using cinder::Rectf;
using cinder::TextBox;
using cinder::app::KeyEvent;
using snake::Direction;
using snake::Location;
using snake::Segment;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::string;

// Global variable
cinder::audio::VoiceRef background_music;
cinder::audio::VoiceRef eating_sound;

const double kRate = 25;
const size_t kLimit = 3;
const char kDbPath[] = "snake.db";
const seconds kCountdownTime = seconds(10);
const size_t kToMilliseconds = 1000;
const size_t kResetColorTime = -100;

#if defined(CINDER_COCOA_TOUCH)
const char kNormalFont[] = "Arial";
const char kBoldFont[] = "Arial-BoldMT";
const char kDifferentFont[] = "AmericanTypewriter";
#elif defined(CINDER_LINUX)
const char kNormalFont[] = "Arial Unicode MS";
const char kBoldFont[] = "Arial Unicode MS";
const char kDifferentFont[] = "Purisa";
#else
const char kNormalFont[] = "Arial";
const char kBoldFont[] = "Arial Bold";
const char kDifferentFont[] = "Papyrus";
#endif

DECLARE_uint32(size);
DECLARE_uint32(tilesize);
DECLARE_uint32(speed);
DECLARE_string(name);

SnakeApp::SnakeApp()
    : engine_{FLAGS_size, FLAGS_size},
      leaderboard_{cinder::app::getAssetPath(kDbPath).string()},
      paused_{false},
      player_name_{FLAGS_name},
      printed_game_over_{false},
      size_{FLAGS_size},
      speed_{FLAGS_speed},
      state_{GameState::kPlaying},
      tile_size_{FLAGS_tilesize},
      time_left_{0},
      color_change_time_{-100},
      color_index_one_{0},
      color_index_two_{1},
      color_index_three_{0},
      snake_size_{1},
      snake_index_one_{0},
      snake_index_two_{0},
      snake_index_three_{1} {}

void SnakeApp::setup() {
  cinder::gl::enableDepthWrite();
  cinder::gl::enableDepthRead();
  // Get audio to play
  cinder::audio::SourceFileRef sourceFile = cinder::audio::load
      (cinder::app::loadAsset("Tetris.mp3"));
  background_music = cinder::audio::Voice::create(sourceFile);
  background_music->start();
}

void SnakeApp::update() {
  if (state_ == GameState::kGameOver) {
    if (top_players_.empty()) {
      leaderboard_.AddScoreToLeaderBoard({player_name_,
                                          engine_.GetScore()});
      top_players_ = leaderboard_.RetrieveHighScores(kLimit);
      // Add your_top_scores_
      your_top_scores_ = leaderboard_.RetrieveHighScores
          (snake::Player(player_name_, engine_.GetScore()), kLimit);

      // It is crucial the this vector be populated, given that `kLimit` > 0.
      assert(!top_players_.empty());
      assert(!your_top_scores_.empty());
    }
    return;
  }

  if (paused_) return;

  const auto time = system_clock::now();

  using std::chrono::seconds;

  // Handle color changing
  double snake_size_now = engine_.GetSnake().Size(); // Get snake size
  double color_duration = 1 / snake_size_now;
  if (color_change_time_ == kResetColorTime) {
    color_change_time_ = color_duration; // set the color changing time
    last_color_time_ = time; // Initialize the "start time"
  }
  // Convert to milliseconds and then int
  double color_duration_double = color_change_time_ * kToMilliseconds;
  int color_duration_int = (int) color_duration_double;
  auto color_duration_ms = milliseconds(color_duration_int);
  // Find the amt of time spend so far
  const auto time_in_color = time - last_color_time_;
  if (time_in_color >= color_duration_ms) {
    // Reset color_change_time_ and generate random numbers for color
    color_change_time_ = kResetColorTime;
    color_index_one_ = ((double) rand() / (RAND_MAX));
    color_index_two_ = ((double) rand() / (RAND_MAX));
    color_index_three_ = ((double) rand() / (RAND_MAX));
  }

  // Handle eating sound --> play noise when the snake size has increased
  if (snake_size_now > snake_size_) {
    cinder::audio::SourceFileRef sourceFile = cinder::audio::load
        (cinder::app::loadAsset("apple_bite.mp3"));
    eating_sound = cinder::audio::Voice::create(sourceFile);
    eating_sound->start();
    snake_size_ = snake_size_now;
  }

  if (engine_.GetSnake().IsChopped()) {
    if (state_ != GameState::kCountDown) {
      state_ = GameState::kCountDown;
      last_intact_time_ = time;
    }

    // We must be in countdown.
    const auto time_in_countdown = time - last_intact_time_;
    if (time_in_countdown >= kCountdownTime) {
      state_ = GameState::kGameOver;
    }


    const auto time_left_s =
        duration_cast<seconds>(kCountdownTime - time_in_countdown);
    time_left_ = static_cast<size_t>(
        std::min(kCountdownTime.count() - 1, time_left_s.count()));
  }

  if (time - last_time_ > std::chrono::milliseconds(speed_)) {
    engine_.Step();
    last_time_ = time;
  }
}

void SnakeApp::draw() {
  cinder::gl::enableAlphaBlending();

  if (state_ == GameState::kGameOver) {
    if (!printed_game_over_) cinder::gl::clear(Color(1, 0, 0));
    DrawGameOver();
    return;
  }

  if (paused_) return;

  cinder::gl::clear();
  DrawBackground();
  DrawSnake();
  DrawFood();
  if (state_ == GameState::kCountDown) DrawCountDown();
}

template <typename C>
void PrintText(const string& text, const C& color, const cinder::ivec2& size,
               const cinder::vec2& loc) {
  cinder::gl::color(color);

  auto box = TextBox()
                 .alignment(TextBox::CENTER)
                 .font(cinder::Font(kNormalFont, 30))
                 .size(size)
                 .color(color)
                 .backgroundColor(ColorA(0, 0, 0, 0))
                 .text(text);

  const auto box_size = box.getSize();
  const cinder::vec2 locp = {loc.x - box_size.x / 2, loc.y - box_size.y / 2};
  const auto surface = box.render();
  const auto texture = cinder::gl::Texture::create(surface);
  cinder::gl::draw(texture, locp);
}

float SnakeApp::PercentageOver() const {
  if (state_ != GameState::kCountDown) return 0.;

  using std::chrono::milliseconds;
  const double elapsed_time =
      duration_cast<milliseconds>(system_clock::now() - last_intact_time_)
          .count();
  const double countdown_time = milliseconds(kCountdownTime).count();
  const double percentage = elapsed_time / countdown_time;
  return static_cast<float>(percentage);
}

void SnakeApp::DrawBackground() const {
  const float percentage = PercentageOver();
  cinder::gl::clear(Color(percentage, 0, 0));
}

void SnakeApp::DrawGameOver() {
  // Lazily print.
  if (printed_game_over_) return;
  if (top_players_.empty()) return;

  const cinder::vec2 center = getWindowCenter();
  const cinder::ivec2 size = {500, 50};
  const Color color = Color::black();

  size_t row = 0;
  PrintText("Game Over :(", color, size, center);
  for (const snake::Player& player : top_players_) {
    std::stringstream ss;
    ss << player.name << " - " << player.score;
    PrintText(ss.str(), color, size,
        {center.x, center.y + (++row) * 50});
  }
  PrintText("Your high scores:", color, size,
      {center.x, center.y + (++row) * 50});
  for (const snake::Player& player : your_top_scores_) {
    std::stringstream ss;
    ss << player.name << " - " << player.score;
    PrintText(ss.str(), color, size,
        {center.x, center.y + (++row) * 50});
  }
  printed_game_over_ = true;
}

void SnakeApp::DrawSnake() const {
  int num_visible = 0;
  for (const Segment& part : engine_.GetSnake()) {
    const Location loc = part.GetLocation();
    if (part.IsVisibile()) {
      const double opacity = std::exp(-(num_visible++) / kRate);
      cinder::gl::color(ColorA(snake_index_one_, snake_index_two_,
          snake_index_three_, static_cast<float>(opacity)));
    } else {
      const float percentage = PercentageOver();
      cinder::gl::color(Color(percentage, 0, 0));
    }

    cinder::gl::drawSolidRect(Rectf(tile_size_ * loc.Row(),
                                    tile_size_ * loc.Col(),
                                    tile_size_ * loc.Row() + tile_size_,
                                    tile_size_ * loc.Col() + tile_size_));
  }
  const cinder::vec2 center = getWindowCenter();
}

void SnakeApp::DrawFood() const {
  // Use color indeces
  cinder::gl::color(color_index_one_, color_index_two_, color_index_three_);

  const Location loc = engine_.GetFood().GetLocation();
  cinder::gl::drawSolidRect(Rectf(tile_size_ * loc.Row(),
                                   tile_size_ * loc.Col(),
                                  tile_size_ * loc.Row() + tile_size_,
                                  tile_size_ * loc.Col() + tile_size_));
}

void SnakeApp::DrawCountDown() const {
  const float percentage = PercentageOver();
  const string text = std::to_string(time_left_);
  const Color color = {1 - percentage, 0, 0};
  const cinder::ivec2 size = {50, 50};
  const cinder::vec2 loc = {50, 50};

  PrintText(text, color, size, loc);
}

void SnakeApp::keyDown(KeyEvent event) {
  switch (event.getCode()) {
    case KeyEvent::KEY_UP:
    case KeyEvent::KEY_k:
    case KeyEvent::KEY_w: {
      engine_.SetDirection(Direction::kLeft);
      break;
    }
    case KeyEvent::KEY_DOWN:
    case KeyEvent::KEY_j:
    case KeyEvent::KEY_s: {
      engine_.SetDirection(Direction::kRight);
      break;
    }
    case KeyEvent::KEY_LEFT:
    case KeyEvent::KEY_h:
    case KeyEvent::KEY_a: {
      engine_.SetDirection(Direction::kUp);
      break;
    }
    case KeyEvent::KEY_RIGHT:
    case KeyEvent::KEY_l:
    case KeyEvent::KEY_d: {
      engine_.SetDirection(Direction::kDown);
      break;
    }
    case KeyEvent::KEY_p: {
      paused_ = !paused_;

      if (paused_) {
        last_pause_time_ = system_clock::now();
      } else {
        last_intact_time_ += system_clock::now() - last_pause_time_;
      }
      break;
    }
    case KeyEvent::KEY_r: {
      ResetGame();
      break;
    }
  }
}

void SnakeApp::mouseDown(cinder::app::MouseEvent event) {
  // Allow user to control game using mouse instead
  if (event.isLeftDown()) {
    engine_.SetDirection(Direction::kUp);
  } else if (event.isRightDown()) {
    engine_.SetDirection(Direction::kDown);
  }

}

void SnakeApp::mouseWheel(cinder::app::MouseEvent event) {
  // Wheel to check to move the snake up or down
  if (event.getWheelIncrement() > 0) {
    engine_.SetDirection(Direction::kRight);
  } else if (event.getWheelIncrement() < 0) {
    engine_.SetDirection(Direction::kLeft);
  }
}

void SnakeApp::ResetGame() {
  engine_.Reset();
  paused_ = false;
  printed_game_over_ = false;
  state_ = GameState::kPlaying;
  time_left_ = 0;
  top_players_.clear();
  your_top_scores_.clear();
}

}  // namespace snakeapp
