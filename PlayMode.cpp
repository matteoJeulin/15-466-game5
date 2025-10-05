#include "PlayMode.hpp"

#include "Mesh.hpp"
#include "Load.hpp"
#include "LitColorTextureProgram.hpp"
#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

GLuint pong_meshes_for_lit_color_texture_program = 0;
Load<MeshBuffer> pong_meshes(LoadTagDefault, []() -> MeshBuffer const *
							 {
	MeshBuffer const *ret = new MeshBuffer(data_path("pong.pnct"));
	pong_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret; });

Load<Scene> pong_scene(LoadTagDefault, []() -> Scene const *
					   { return new Scene(data_path("pong.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name)
										  {
												 Mesh const &mesh = pong_meshes->lookup(mesh_name);

												 scene.drawables.emplace_back(transform);
												 Scene::Drawable &drawable = scene.drawables.back();

												 drawable.pipeline = lit_color_texture_program_pipeline;

												 drawable.pipeline.vao = pong_meshes_for_lit_color_texture_program;
												 drawable.pipeline.type = mesh.type;
												 drawable.pipeline.start = mesh.start;
												 drawable.pipeline.count = mesh.count; }); });

PlayMode::PlayMode(Client &client_) : scene(*pong_scene), client(client_)
{
	// get pointers to leg for convenience:
	for (auto &transform : scene.transforms)
	{
		if (transform.name == "PaddleLeft")
			paddleLeft = &transform;
		else if (transform.name == "PaddleRight")
			paddleRight = &transform;
		else if (transform.name == "Ball")
			ball = &transform;
		else if (transform.name == "WallTop")
			wallTop = &transform;
		else if (transform.name == "WallBottom")
			wallBottom = &transform;
		else if (transform.name == "WallLeft")
			wallLeft = &transform;
		else if (transform.name == "WallRight")
			wallRight = &transform;
		else if (transform.name == "PowerUpPad")
			powerUpPad = &transform;
	}

	if (paddleLeft == nullptr)
		throw std::runtime_error("Left paddle not found.");
	if (paddleRight == nullptr)
		throw std::runtime_error("Right paddle not found.");
	if (ball == nullptr)
		throw std::runtime_error("Ball not found.");
	if (wallTop == nullptr)
		throw std::runtime_error("Top wall not found.");
	if (wallBottom == nullptr)
		throw std::runtime_error("Bottom wall not found.");
	if (wallLeft == nullptr)
		throw std::runtime_error("Left wall not found.");
	if (wallRight == nullptr)
		throw std::runtime_error("Right wall not found.");
	if (powerUpPad == nullptr)
		throw std::runtime_error("Power up pad not found.");

	defaultLeftWallPos = wallLeft->position;
	defaultRightWallPos = wallRight->position;

	// Move the back walls off screen
	wallRight->position = glm::vec3(1000.0f, 0.0f, 0.0f);
	wallLeft->position = glm::vec3(1000.0f, 0.0f, 0.0f);

	// get pointer to camera for convenience:
	if (scene.cameras.size() != 1)
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{

	if (evt.type == SDL_EVENT_KEY_DOWN)
	{
		if (evt.key.repeat)
		{
			// ignore repeats
		}
		else if (evt.key.key == SDLK_UP || evt.key.key == SDLK_W)
		{
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN || evt.key.key == SDLK_S)
		{
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		}
	}
	else if (evt.type == SDL_EVENT_KEY_UP)
	{
		if (evt.key.key == SDLK_UP || evt.key.key == SDLK_W)
		{
			controls.up.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN || evt.key.key == SDLK_S)
		{
			controls.down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{

	// queue data for sending to server:
	controls.send_controls_message(&client.connection);

	// reset button press counters:
	controls.up.downs = 0;
	controls.down.downs = 0;

	// send/receive data:
	client.poll([this](Connection *c, Connection::Event event)
				{
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		} }, 0.0);

	// Place the paddles
	paddleLeft->position = glm::vec3(-paddlePos, game.players.front().position, paddleLeft->position.z);
	paddleRight->position = glm::vec3(paddlePos, game.players.back().position, paddleRight->position.z);

	// Place the ball
	ball->position = glm::vec3(game.BallPosition, game.BallRadius);

	// Place the power up pad
	{
		if (game.currPowerUp.active)
			powerUpPad->position = glm::vec3(game.currPowerUp.Position, 1.0f);
		else
			powerUpPad->position = DontShow;
	}

	// Place the back walls if the player has an extra life
	{
		if (game.players.front().hasPowerUp(PowerUp::ExtraLife))
			wallRight->position = defaultRightWallPos;
		else
			wallRight->position = DontShow;

		if (game.players.back().hasPowerUp(PowerUp::ExtraLife))
			wallLeft->position = defaultLeftWallPos;
		else
			wallLeft->position = DontShow;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
	// update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	// set up light type and position for lit_color_texture_program:
	//  TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); // this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	std::string score_str = std::to_string(game.players.front().score) + " - " + std::to_string(game.players.back().score);
	tm.draw_text(score_str, drawable_size, glm::vec2(drawable_size.x / 2.0f, 36), glm::vec3(0.0f, 0.0f, 0.0f));

	GL_ERRORS();
}
