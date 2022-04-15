#include <dragonstd/map.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "day.h"
#include "entity.h"
#include "perlin.h"
#include "server/database.h"
#include "server/server_config.h"
#include "server/server_player.h"
#include "server/server_terrain.h"

static Map players;
static Map players_named;

// main thread
// called on server shutdown
static void player_drop(ServerPlayer *player)
{
	pthread_rwlock_rdlock(&player->lock_peer);
	pthread_t recv_thread = player->peer ? player->peer->recv_thread : 0;
	pthread_rwlock_unlock(&player->lock_peer);

	server_player_disconnect(player);
	if (recv_thread)
		pthread_join(recv_thread, NULL);

	refcount_drp(&player->rc); // map no longer has a reference to player
}

// any thread
// called when all refs have been dropped
static void player_delete(ServerPlayer *player)
{
	refcount_dst(&player->rc);

	pthread_rwlock_destroy(&player->lock_peer);

	free(player->name);
	pthread_rwlock_destroy(&player->lock_auth);

	pthread_rwlock_destroy(&player->lock_pos);

	free(player);
}

// recv thread
// called when auth was successful
static void player_spawn(ServerPlayer *player)
{
	// lock_pos has already been wrlocked by caller
	if (!database_load_player(player->name, &player->pos, &player->rot)) {
		player->pos = (v3f64) {0.0, server_terrain_spawn_height() + 0.5, 0.0};
		player->rot = (v3f32) {0.0f, 0.0f, 0.0f};
		database_create_player(player->name, player->pos, player->rot);
	}

	// since this is recv thread, we don't need lock_peer
	dragonnet_peer_send_ToClientInfo(player->peer, &(ToClientInfo) {
		.seed = seed,
		.load_distance = server_config.load_distance,
	});
	dragonnet_peer_send_ToClientTimeOfDay(player->peer, &(ToClientTimeOfDay) {
		.time_of_day = get_time_of_day(),
	});
	dragonnet_peer_send_ToClientMovement(player->peer, &(ToClientMovement) {
		.flight = false,
		.collision = true,
		.speed = server_config.movement.speed_normal,
		.gravity = server_config.movement.gravity,
		.jump = server_config.movement.jump,
	});
	dragonnet_peer_send_ToClientEntityAdd(player->peer, &(ToClientEntityAdd) {
		.type = ENTITY_LOCALPLAYER,
		.data = {
			.id = player->id,
			.pos = player->pos,
			.rot = player->rot,
			.box = {{-0.3f, 0.0f, -0.3f}, {0.3f, 1.75f, 0.3f}},
			.eye = {0.0f, 1.75f, 0.0f},
			.nametag = NULL,
		},
	});
}

// any thread
// called when adding, getting or removing a player from the map
static int cmp_player_id(const Refcount *player, const u64 *id)
{
	return u64_cmp(&((ServerPlayer *) player->obj)->id, id);
}

// any thread
// called when adding, getting or removing a player from the players_named Map
static int cmp_player_name(const Refcount *player, const char *name)
{
	// names of players in players_named Map don't change, no lock_auth needed
	return strcmp(((ServerPlayer *) player->obj)->name, name);
}

// main thread
// called on server startup
void server_player_init()
{
	map_ini(&players);
	map_ini(&players_named);
}

// main thread
// called on server shutdown
void server_player_deinit()
{
	// just forget about name -> player mapping
	map_cnl(&players_named, &refcount_drp, NULL, NULL,          0);
	// disconnect players and forget about them
	map_cnl(&players,       &player_drop,  NULL, &refcount_obj, 0);
}

// accept thread
// called on new connection
void server_player_add(DragonnetPeer *peer)
{
	ServerPlayer *player = malloc(sizeof *player);

	// ID is allocated later in this function
	player->id = 0;
	refcount_ini(&player->rc, player, &player_delete);

	player->peer = peer;
	pthread_rwlock_init(&player->lock_peer, NULL);

	player->auth = false;
	// use address as name until auth is done
	player->name = dragonnet_addr_str(peer->raddr);
	pthread_rwlock_init(&player->lock_auth, NULL);

	player->pos = (v3f64) {0.0f, 0.0f, 0.0f};
	player->rot = (v3f32) {0.0f, 0.0f, 0.0f};
	pthread_rwlock_init(&player->lock_pos, NULL);

	printf("[access] connected %s\n", player->name);
	peer->extra = refcount_grb(&player->rc);

	// keep the search tree somewhat balanced by using random IDs
	// duplicate IDs are very unlikely, but it doesn't hurt to check
	// make sure to avoid 0 since it's not a valid ID
	while (!player->id || !map_add(&players, &player->id, &player->rc, &cmp_player_id, &refcount_inc))
		player->id = random();

	// player has been grabbed by Map and peer
	refcount_drp(&player->rc);
}

// recv thread
// called on connection close
void server_player_remove(DragonnetPeer *peer)
{
	ServerPlayer *player = peer->extra;
	peer->extra = NULL; // technically not necessary, but just in case

	// peer will be deleted - forget about it!
	pthread_rwlock_wrlock(&player->lock_peer);
	player->peer = NULL;
	pthread_rwlock_unlock(&player->lock_peer);

	// only (this) recv thread will modify the auth or name fields, no lock_auth needed
	// map_del returns false if it was canceled
	// (we don't want disconnect messages for every player on server shutdown)
	if (map_del(&players, &player->id, &cmp_player_id, &refcount_drp, NULL, NULL))
		printf("[access] disconnected %s\n", player->name);
	if (player->auth)
		map_del(&players_named, player->name, &cmp_player_name, &refcount_drp, NULL, NULL);

	// peer no longer has a reference to player
	refcount_drp(&player->rc);
}

// any thread
ServerPlayer *server_player_grab(u64 id)
{
	// 0 is an invalid ID
	return id ? map_get(&players, &id, &cmp_player_id, &refcount_grb) : NULL;
}

// any thread
ServerPlayer *server_player_grab_named(char *name)
{
	// NULL is an invalid name
	return name ? map_get(&players, name, &cmp_player_name, &refcount_grb) : NULL;
}

// recv thread
// called on recv auth packet
bool server_player_auth(ServerPlayer *player, char *name)
{
	pthread_rwlock_wrlock(&player->lock_auth);
	pthread_rwlock_wrlock(&player->lock_pos);

	// temporary change name, save old name to either free or reset it if auth fails
	char *old_name = player->name;
	player->name = name;

	bool success = map_add(&players_named, player->name, &player->rc, &cmp_player_name, &refcount_inc);

	printf("[access] authentication %s: %s -> %s\n", success ? "success" : "failure", old_name, player->name);

	// since this is recv thread, we don't need lock_peer
	dragonnet_peer_send_ToClientAuth(player->peer, &(ToClientAuth) {
		.success = success,
	});

	if (success) {
		free(old_name);
		player->auth = true;
		// load player from database and send some initial info
		player_spawn(player);
	} else {
		player->name = old_name;
	}

	pthread_rwlock_unlock(&player->lock_pos);
	pthread_rwlock_unlock(&player->lock_auth);
	return success;
}

// any thread
void server_player_disconnect(ServerPlayer *player)
{
	pthread_rwlock_rdlock(&player->lock_peer);
	// the recv thread will call server_player_remove when the connection was shut down
	if (player->peer)
		dragonnet_peer_shutdown(player->peer);
	pthread_rwlock_unlock(&player->lock_peer);
}

// any thread
void server_player_iterate(void *func, void *arg)
{
	map_trv(&players_named, func, arg, &refcount_obj, TRAVERSION_INORDER);
}

/*
229779
373875
374193
110738
390402
357272
390480

(these are only the wholesome ones)
*/