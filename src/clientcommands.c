#include <stdio.h>
#include <unistd.h>
#include "client.h"
#include "clientmap.h"
#include "types.h"

static bool disconnect_handler(Client *client, bool good)
{
	(void) client;

	if (good)
		client_disconnect(false, NULL);
	return true;
}

static bool auth_handler(Client *client, bool good)
{
	u8 success;
	if (! read_u8(client->fd, &success))
		return false;

	if (! good)
		return true;

	if (success) {
		printf("Authenticated successfully\n");
		client->state = CS_ACTIVE;
	} else {
		printf("Authentication failed, please try again\n");
		client->state = CS_CREATED;
	}

	return true;
}

static bool block_handler(Client *client, bool good)
{
	v3s32 pos;

	if (! read_v3s32(client->fd, &pos))
		return false;

	MapBlockHeader header;

	if (! read_u16(client->fd, &header))
		return false;

	char data[header];
	size_t n_read_total = 0;
	int n_read;
	while (n_read_total < header) {
		if ((n_read = read(client->fd, data + n_read_total, header - n_read_total)) == -1) {
			perror("read");
			return false;
		}
		n_read_total += n_read;
	}

	MapBlock *block;

	if (good)
		block = map_get_block(client->map, pos, true);
	else
		block = map_allocate_block(pos);

	if (block->state != MBS_CREATED)
		map_clear_meta(block);

	bool ret = map_deserialize_block(block, data, header);

	if (good)
		clientmap_block_changed(block);
	else
		map_free_block(block);

	return ret;
}

CommandHandler command_handlers[CLIENT_COMMAND_COUNT] = {
	{0},
	{&disconnect_handler, "DISCONNECT", CS_CREATED | CS_AUTH | CS_ACTIVE},
	{&auth_handler, "AUTH", CS_AUTH},
	{&block_handler, "BLOCK", CS_ACTIVE},
};
