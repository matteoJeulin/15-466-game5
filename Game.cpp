#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const
{
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 2;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b)
	{
		if (b.downs & 0x80)
		{
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t((b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f)));
	};

	send_button(up);
	send_button(down);
}

bool Player::Controls::recv_controls_message(Connection *connection_)
{
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	// expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4)
		return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls))
		return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16) | (uint32_t(recv_buffer[2]) << 8) | uint32_t(recv_buffer[1]);
	if (size != 2)
		throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 5!");

	// expecting complete message:
	if (recv_buffer.size() < 4 + size)
		return false;

	auto recv_button = [](uint8_t byte, Button *button)
	{
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255)
		{
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	recv_button(recv_buffer[4 + 0], &up);
	recv_button(recv_buffer[4 + 1], &down);

	// delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}

bool Player::hasPowerUp(PowerUp::Type powerUp)
{
	return std::find(powerUps.begin(), powerUps.end(), powerUp) != powerUps.end();
}

//-----------------------------------------

Game::Game() { start_round(); }

void Game::start_round()
{
	// Set the ball's direction to a random direction
	std::random_device rd;
	std::mt19937 mt_dir(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	BallDirection.x = dist(mt_dir);
	BallDirection.y = dist(mt_dir);

	// Make sure the ball is not going straight up or down or doesn't move
	while (BallDirection.x == 0.0f && (std::abs(BallDirection.y) == 1.0f || BallDirection.y == 0.0f))
	{
		BallDirection.x = dist(mt_dir);
		BallDirection.y = dist(mt_dir);
	};

	BallDirection = glm::normalize(BallDirection);
	BallPosition = glm::vec2(0.0f, 0.0f);

	currBallSpeed = BallSpeed;
}

Player *Game::spawn_player()
{
	players.emplace_back();
	Player &player = players.back();

	player.name = "Player " + std::to_string(next_player_number++);

	// Reset ball position when a new player arrives
	BallPosition = glm::vec2(0.0f);

	return &player;
}

void Game::remove_player(Player *player)
{
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi)
	{
		if (&*pi == player)
		{
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed)
{
	if (!players.empty())
	{
		// Power ups
		{
			if (!currPowerUp.active)
				currPowerUpCooldown -= elapsed;

			if (currPowerUpCooldown < 0)
			{
				currPowerUp.active = true;
				std::random_device rd;
				std::mt19937 mt_dir(rd());
				std::uniform_real_distribution<float> dist(-0.8f, 0.8f);

				currPowerUp.Position = glm::vec2(ArenaMax.x * dist(mt_dir), ArenaMax.y * dist(mt_dir));

				std::uniform_int_distribution<int> dist_pu(0, PowerUp::TYPE_LENGTH);

				currPowerUp.type = static_cast<PowerUp::Type>(dist_pu(mt_dir));

				currPowerUpCooldown = PowerUpCooldown;
			}
		}

		// position/velocity update for players:
		for (auto &p : players)
		{
			float dir = 0.0f;
			if (p.controls.down.pressed)
				dir -= 1.0f;
			if (p.controls.up.pressed)
				dir += 1.0f;

			if (dir == 0.0f)
			{
				// no inputs: just drift to a stop
				float amt = 1.0f - std::pow(0.5f, elapsed / (PlayerAccelHalflife * 2.0f));
				p.velocity = glm::mix(p.velocity, 0.0f, amt);
			}
			else
			{
				float amt = 1.0f - std::pow(0.5f, elapsed / PlayerAccelHalflife);

				// accelerate along velocity (if not fast enough):
				float along = dir * p.velocity;
				if (along < PlayerSpeed)
				{
					along = glm::mix(along, PlayerSpeed, amt);
				}

				p.velocity = dir * along;
			}
			p.position += p.velocity * elapsed;

			// reset 'downs' since controls have been handled:
			p.controls.up.downs = 0;
			p.controls.down.downs = 0;
		}

		// position update for the ball:
		{
			prevBallPosition = BallPosition;
			BallPosition += BallDirection * BallSpeed * elapsed;
		}

		// collision resolution:
		for (auto &p : players)
		{
			if (p.position - PlayerHeight < ArenaMin.y)
			{
				p.position = ArenaMin.y + PlayerHeight;
			}
			if (p.position + PlayerHeight > ArenaMax.y)
			{
				p.position = ArenaMax.y - PlayerHeight;
			}
		}

		Player &receivingPlayer = BallDirection.x < 0 ? players.front() : players.back();
		Player &senderPlayer = BallDirection.x > 0 ? players.front() : players.back();
		float playerSide = std::copysignf(1.0f, BallDirection.x);

		// Ball collision with the walls
		if (BallPosition.y - BallRadius < ArenaMin.y + WallThickness || BallPosition.y + BallRadius > ArenaMax.y - WallThickness)
		{
			BallDirection.y = -BallDirection.y;
		}
		if (BallPosition.x - BallRadius < ArenaMin.x + WallThickness || BallPosition.x + BallRadius > ArenaMax.x - WallThickness)
		{
			// Only score if the receiving player doesn't have an extra life
			if (!receivingPlayer.hasPowerUp(PowerUp::Type::ExtraLife))
			{
				senderPlayer.score++;
				start_round();
			}
			else
			{
				auto extraLifePowerUp = std::find(receivingPlayer.powerUps.begin(), receivingPlayer.powerUps.end(), PowerUp::ExtraLife);
				receivingPlayer.powerUps.erase(extraLifePowerUp);
			}
		}

		// Ball collision with power up pad
		if (currPowerUp.active) {
			if (BallPosition.y - BallRadius < currPowerUp.Position.y + PowerUpPadSize.y &&
				BallPosition.y + BallRadius > currPowerUp.Position.y - PowerUpPadSize.y &&
				BallPosition.x + playerSide * BallRadius > currPowerUp.Position.x - PowerUpPadSize.x &&
				BallPosition.x + playerSide * BallRadius < currPowerUp.Position.x + PowerUpPadSize.x)
			{
				currPowerUp.active = false;

				if (currPowerUp.type == PowerUp::SpeedUp)
					currBallSpeed *= BallSpeedUpFactor;
				else
					senderPlayer.powerUps.push_back(currPowerUp.type);
			}
		}

		// Ball collision with the paddles
		{
			if (BallPosition.y - BallRadius < receivingPlayer.position + PlayerHeight &&
				BallPosition.y + BallRadius > receivingPlayer.position - PlayerHeight &&
				BallPosition.x + playerSide * BallRadius > playerSide * PlayerXPos - PlayerWidth &&
				BallPosition.x + playerSide * BallRadius < playerSide * PlayerXPos + PlayerWidth)
			{
				// Bounce off the side
				if (prevBallPosition.x + playerSide * BallRadius < playerSide * PlayerXPos - PlayerWidth ||
					prevBallPosition.x + playerSide * BallRadius > playerSide * PlayerXPos + PlayerWidth)
				{
					BallDirection.x = -BallDirection.x;
					BallDirection.y += receivingPlayer.velocity * FrictionFactor;
				}

				// Bounce off the top/bottom
				if ((prevBallPosition.y - BallRadius > receivingPlayer.position + PlayerHeight ||
					 prevBallPosition.y + BallRadius < receivingPlayer.position - PlayerHeight))
					BallDirection.y = -BallDirection.y;
			}
		}
	}
}

void Game::send_state_message(Connection *connection_, Player *connection_player) const
{
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	// will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); // keep track of this position in the buffer

	// send player info helper:
	auto send_player = [&](Player const &player)
	{
		connection.send(player.position);
		connection.send(player.score);
		connection.send(player.powerUps.size());
		if (player.powerUps.size() > 0)
			connection.send_buffer.insert(connection.send_buffer.end(), player.powerUps.begin(), player.powerUps.end());

		// NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		// effectively: truncates player name to 255 chars
		uint8_t len = uint8_t(std::min<size_t>(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	// player count:
	connection.send(uint8_t(players.size()));
	if (connection_player)
		send_player(*connection_player);
	for (auto const &player : players)
	{
		if (&player == connection_player)
			continue;
		send_player(player);
	}

	connection.send(BallPosition);
	connection.send(currPowerUp.active);
	connection.send(currPowerUp.Position);

	// compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark - 3] = uint8_t(size);
	connection.send_buffer[mark - 2] = uint8_t(size >> 8);
	connection.send_buffer[mark - 1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_)
{
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4)
		return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State))
		return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16) | (uint32_t(recv_buffer[2]) << 8) | uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	// expecting complete message:
	if (recv_buffer.size() < 4 + size)
		return false;

	// copy bytes from buffer and advance position:
	auto read = [&](auto *val)
	{
		if (at + sizeof(*val) > size)
		{
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i)
	{
		players.emplace_back();
		Player &player = players.back();
		read(&player.position);
		read(&player.score);
		size_t powerUpsLength;
		read(&powerUpsLength);
		player.powerUps = {};
		if (powerUpsLength > 0)
		{
			for (size_t n = 0; n < powerUpsLength; ++n)
			{
				PowerUp::Type p;
				read(&p);
				player.powerUps.emplace_back(p);
			}
		}

		uint8_t name_len;
		read(&name_len);
		// n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		player.name = "";
		for (uint8_t n = 0; n < name_len; ++n)
		{
			char c;
			read(&c);
			player.name += c;
		}
	}

	read(&BallPosition);
	read(&currPowerUp.active);
	read(&currPowerUp.Position);

	if (at != size)
		throw std::runtime_error("Trailing data in state message.");

	// delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
