#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

struct PowerUp {
	enum Type {
		ExtraLife, // Add an extra life to the player
		Freeze, // Freeze enemy player
		SpeedUp, // Speed up the ball
		TYPE_LENGTH // Number of power up types
	};

	// Type of the power up
	Type type;

	// Boolean indicating if the power up pad is currently on the arena
	bool active = false;

	// Position of the power up pad in the arena
	glm::vec2 Position = glm::vec2(0.0f, 0.0f);
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		Button up, down;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	// Power ups the player currently has
	std::vector<PowerUp::Type> powerUps;

	bool hasPowerUp(PowerUp::Type powerUp);

	//player state (sent from server):
	float position = 0.0f;
	float velocity = 0.0f;

	inline static constexpr float FreezeTimer = 1.0f;
	float currFreezeTimer = FreezeTimer;
	
	uint32_t score = 0;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	void start_round();

	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//arena size:
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-160.0f, -90.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 160.0f,  90.0f);
	inline static constexpr float WallThickness = 3.0f;

	//player constants:
	inline static constexpr float PlayerSpeed = 50.0f;
	inline static constexpr float PlayerWidth = 2.0f;
	inline static constexpr float PlayerHeight = 10.0f;
	inline static constexpr float PlayerXPos = 140.0f;
	inline static constexpr float PlayerAccelHalflife = 0.05f;

	//ball constants:
	inline static constexpr float BallRadius = 3.0f;
	inline static constexpr float BallSpeed = 100.0f;
	inline static constexpr float FrictionFactor = 0.1f;


	//ball movement:
	glm::vec2 BallPosition = glm::vec2(0.0f, 0.0f);
	glm::vec2 BallDirection = glm::vec2(0.0f, 0.0f);
	glm::vec2 prevBallPosition = glm::vec2(0.0f, 0.0f);
	float currBallSpeed = BallSpeed;

	//power up constants:
	inline static constexpr float PowerUpCooldown = 1.0f;
	inline static constexpr glm::vec2 PowerUpPadSize = glm::vec2(10.0f, 10.0f);
	inline static constexpr float BallSpeedUpFactor = 1.5f;

	//power ups:
	PowerUp currPowerUp;
	float currPowerUpCooldown = PowerUpCooldown;

	//used for player creation:

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
