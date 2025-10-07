#include "Mode.hpp"

#include "Connection.hpp"
#include "Game.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "TextManager.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//text display
	TextManager tm = TextManager();

	// local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform *paddleRight = nullptr;
	Scene::Transform *paddleLeft = nullptr;

	Scene::Transform *ball = nullptr;
	Scene::Transform *wallLeft = nullptr;
	Scene::Transform *wallRight = nullptr;
	Scene::Transform *wallTop = nullptr;
	Scene::Transform *wallBottom = nullptr;
	Scene::Transform *powerUpPad = nullptr;

	glm::vec3 defaultRightWallPos;
	glm::vec3 defaultLeftWallPos;

	float paddlePos = 140;

	inline static constexpr glm::vec3 DontShow = glm::vec3(1000.0f, 1000.0f, 1000.0f);

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	// Sounds:
	std::shared_ptr<Sound::PlayingSample> oneshots[8];
	std::vector<Sound::Sample> samples;

	std::shared_ptr<Sound::PlayingSample> music_loop;

	// camera:
	Scene::Camera *camera = nullptr;
};
