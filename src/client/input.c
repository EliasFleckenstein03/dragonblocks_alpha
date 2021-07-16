#include <math.h>
#include "client/camera.h"
#include "client/client.h"
#include "client/client_player.h"
#include "client/hud.h"
#include "client/input.h"
#include "client/window.h"

typedef struct
{
	int key;
	bool was_pressed;
	bool fired;
} KeyListener;

static struct
{
	HUDElement *pause_menu_hud;
	bool paused;
	KeyListener pause_listener;
	KeyListener fullscreen_listener;
} input;

void input_on_cursor_pos(double current_x, double current_y)
{
	if (input.paused)
		return;

	static double last_x, last_y = 0.0;

	double delta_x = current_x - last_x;
	double delta_y = current_y - last_y;
	last_x = current_x;
	last_y = current_y;

	client_player.yaw += (f32) delta_x * M_PI / 180.0f / 8.0f;
	client_player.pitch -= (f32) delta_y * M_PI / 180.0f / 8.0f;

	client_player.pitch = fmax(fmin(client_player.pitch, M_PI / 2.0f - 0.01f), -M_PI / 2.0f + 0.01f);

	camera_set_angle(client_player.yaw, client_player.pitch);
}

static bool move(int forward, int backward, vec3 dir)
{
	f32 sign;
	f32 speed = 4.317f;

	if (glfwGetKey(window.handle, forward) == GLFW_PRESS)
		sign = +1.0f;
	else if (glfwGetKey(window.handle, backward) == GLFW_PRESS)
		sign = -1.0f;
	else
		return false;

	client_player.velocity.x += dir[0] * speed * sign;
	client_player.velocity.z += dir[2] * speed * sign;

	return true;
}

static void enter_game()
{
	glfwSetInputMode(window.handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	input.pause_menu_hud->visible = false;
}

static void do_key_listener(KeyListener *listener)
{
	bool is_pressed = glfwGetKey(window.handle, listener->key) == GLFW_PRESS;
	listener->fired = listener->was_pressed && ! is_pressed;
	listener->was_pressed = is_pressed;
}

static KeyListener create_key_listener(int key)
{
	return (KeyListener) {
		.key = key,
		.was_pressed = false,
		.fired = false,
	};
}

void input_tick()
{
	do_key_listener(&input.pause_listener);
	do_key_listener(&input.fullscreen_listener);

	if (input.pause_listener.fired) {
		input.paused = ! input.paused;

		if (input.paused) {
			glfwSetInputMode(window.handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			input.pause_menu_hud->visible = true;
		} else {
			enter_game();
		}
	}

	if (input.fullscreen_listener.fired) {
		if (window.fullscreen)
			window_exit_fullscreen();
		else
			window_enter_fullscreen();
	}

	client_player.velocity.x = 0.0f;
	client_player.velocity.z = 0.0f;

	if (! input.paused) {
		move(GLFW_KEY_W, GLFW_KEY_S, camera_movement_dirs.front);
		move(GLFW_KEY_D, GLFW_KEY_A, camera_movement_dirs.right);

		if (glfwGetKey(window.handle, GLFW_KEY_SPACE) == GLFW_PRESS)
			client_player_jump();
	}
}

void input_init()
{
	input.paused = false;

	input.pause_listener = create_key_listener(GLFW_KEY_ESCAPE);
	input.fullscreen_listener = create_key_listener(GLFW_KEY_F11);

	input.pause_menu_hud = hud_add((HUDElementDefinition) {
		.type = HUD_IMAGE,
		.pos = {-1.0f, -1.0f, 0.5f},
		.offset = {0, 0},
		.type_def = {
			.image = {
				.texture = texture_get(RESSOURCEPATH "textures/pause_layer.png"),
				.scale = {1.0f, 1.0f},
				.scale_type = HUD_SCALE_SCREEN
			},
		},
	});

	glfwSetInputMode(window.handle, GLFW_STICKY_KEYS, GL_TRUE);

	enter_game();
}