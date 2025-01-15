#define SDL_MAIN_HANDLED
#include "Player.hpp"
#include "SDL_render.h"
#include "SDL_timer.h"
#include "asteroid.hpp"
#include "createwindow.hpp"
#include "score.hpp"
#include <optional>
#include <vector>

std::vector<std::unique_ptr<Player>> createPlayers(int count) {
  int centerX = static_cast<int>(SCREEN_WIDTH / 2);
  int bottomY = static_cast<int>(SCREEN_HEIGHT - 100);
  const int PLAYER_SPACING = 70;
  const int VERTICAL_OFFSET = 50;

  std::vector<std::unique_ptr<Player>> players;

  if (count <= 0) {
    return players;
  }

  if (count == 1) {
    // Center on Single element
    players.push_back(std::make_unique<Player>(centerX, bottomY));
  } else {
    // V formation logic when multiple

    int middleIndex = count / 2;
    for (int i = 0; i < count; ++i) {
      int offsetX = (i - middleIndex) * PLAYER_SPACING;
      int offsetY = std::abs(i - middleIndex) * VERTICAL_OFFSET;
      players.push_back(
          std::make_unique<Player>(centerX + offsetX, bottomY - offsetY));
    }
  }

  return players;
}
int main(int argc, char *args[]) {
  if (!init()) {
    printf("Failed to initialize!\n");
    return 1;
  }

  int imgFlags = IMG_INIT_PNG;
  // Initializing SDL_Image w/ png
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    printf("SDL_image could not initialize! SDL_image Error: %s\n",
           IMG_GetError());
    return 1;
  }
  int deathCount = 3;
  Score scoreDisplay;
  auto players = createPlayers(3);

  for (auto &player : players) {
    if (!player->loadTexture("src/spaceship.png", gRenderer)) {
      printf("Failed to load player3 texture\n");
      return 1;
    }
  }
  // ASTEROIDS
  std::vector<Asteroid> asteroids;
  for (int i = 0; i < 19; i++) {
    asteroids.emplace_back(players);
    if (!asteroids.back().loadTexture("src/asteroid.png", gRenderer)) {
      printf("Failed to load asteroid texture for asteroid %d\n", i);
      return 1;
    }
  }

  SDL_Event e;
  bool quit = false;

  // MY TIMESTEP VARS
  const float FIXED_TIME_STEP = 1.0f / 120.0f; //(8.33ms)
  float accumulator = 0.0f;
  Uint32 lastTime = SDL_GetTicks();

  // FRAME RATE CAP
  const float FPS = 120.0f;
  const float frameDelay = 1000.0f / FPS;

  while (!quit) {
    // FRAME TIMING
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    deltaTime = std::min(deltaTime, 0.25f); // Cap max frame time
    accumulator += deltaTime;

    // Handle SDL events
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
    }

    // Get keyboard state
    const Uint8 *keyState = SDL_GetKeyboardState(NULL);

    // TIMESTEP UPDATE LOOP
    while (accumulator >= FIXED_TIME_STEP) {
      // Update player positions based on input
      for (auto &player : players) {
        player->handlePlayerInputAndPosition(keyState);
      }

      // Update asteroids and handle bullet collisions
      std::vector<size_t> asteroidsToRemove;
      for (auto &asteroid : asteroids) {
        SDL_Rect asteroidRect = {asteroid.getRectX(), asteroid.getRectY(),
                                 asteroid.getRectWidth(),
                                 asteroid.getRectHeight()};

        bool asteroidHit = false;
        // BULLET <----> ASTEROID DETECTION
        for (auto &player : players) {
          auto bulletIndex =
              player->getWeapon().checkBulletCollision(asteroidRect);
          if (bulletIndex.has_value()) {
            printf("Bullet %zu hit asteroid at position: %d, %d\n",
                   bulletIndex.value(), asteroid.getRectX(),
                   asteroid.getRectY());
            player->getWeapon().destroyBullet(bulletIndex.value());
            asteroid.destroy();
            scoreDisplay.setScore(100);
            break;
          }
        }
        if (asteroidHit)
          break;
      }

      // MARK DESTROYED ASTEROIDS
      for (size_t i = 0; i < asteroids.size(); i++) {
        if (asteroids[i].isDestroyed()) {
          asteroidsToRemove.push_back(i);
        }
      }

      // REMOVE DESTROYED ASTROIDS IN REVERSE
      for (auto it = asteroidsToRemove.rbegin(); it != asteroidsToRemove.rend();
           ++it) {
        if (*it < asteroids.size()) {
          asteroids.erase(asteroids.begin() + *it);
        }
      }

      // PLAYER <--> ASTEROID DETECTION
      std::vector<Player *> playersToRemove;
      for (const auto &player : players) {
        SDL_Rect playerRect = {player->getPosition().first,
                               player->getPosition().second, player->getWidth(),
                               player->getHeight()};

        for (const auto &asteroid : asteroids) {
          SDL_Rect asteroidRect = {asteroid.getRectX(), asteroid.getRectY(),
                                   asteroid.getRectWidth(),
                                   asteroid.getRectHeight()};

          if (player->checkCollision(playerRect, asteroidRect)) {
            // TODO: Refactor single player respawn logic into separate function
            // TODO: Add proper game over state handling
            // TODO: Consider adding invulnerability frames after respawnj
            if (players.size() == 1) {
              players.clear();
              players = createPlayers(++deathCount);
              for (auto &player : players) {
                if (!player->loadTexture("src/spaceship.png", gRenderer)) {
                  printf("Failed to load player3 texture\n");
                  return 1;
                }
              }
            }
            playersToRemove.push_back(player.get());
            scoreDisplay.setScore(-100);
            break;
          }
        }
      }

      // REMOVE DESTROYED PLAYERS
      players.erase(
          std::remove_if(
              players.begin(), players.end(),
              [&playersToRemove](const std::unique_ptr<Player> &player) {
                return std::find(playersToRemove.begin(), playersToRemove.end(),
                                 player.get()) != playersToRemove.end();
              }),
          players.end());

      // UPDATE ASTEROID POSITIONS
      for (auto &asteroid : asteroids) {
        asteroid.update();
      }

      accumulator -= FIXED_TIME_STEP;
    }

    // RENDERING (HAPPENS EVERY FRAME)
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);

    // RENDER GAME OBJECTS
    scoreDisplay.renderScore(gRenderer);
    for (auto &player : players) {
      player->renderPlayer(gRenderer);
    }
    for (auto &asteroid : asteroids) {
      asteroid.renderAsteroid(gRenderer);
    }

    SDL_RenderPresent(gRenderer);

    // FRAME RATE CAPPING
    Uint32 endTime = SDL_GetTicks();
    float elapsedMS = endTime - currentTime;
    if (frameDelay > elapsedMS) {
      SDL_Delay(frameDelay - elapsedMS);
    }
  }
  close();
  return 0;
}
