#include <stdio.h>
#include <unistd.h>
#include <endian.h>
#include <poll.h>
#include "types.h"

#define htobe8(x) x
#define be8toh(x) x

#define READVEC(type, n) \
	type buf[n]; \
	for (int i = 0; i < n; i++) { \
		if (! read_ ## type(fd, &buf[i])) \
			return false; \
	}

#define WRITEVEC(type, n) \
	for (int i = 0; i < n; i++) { \
		if (! write_ ## type(fd, vec[i])) \
			return false; \
	} \
	return true;

#define DEFVEC(type) \
	bool read_v2 ## type(int fd, v2 ## type *ptr) \
	{ \
		READVEC(type, 2) \
		ptr->x = buf[0]; \
		ptr->y = buf[1]; \
		return true; \
	} \
	bool write_v2 ## type(int fd, v2 ## type val) \
	{ \
		type vec[2] = {val.x, val.y}; \
		WRITEVEC(type, 2) \
	} \
	bool read_v3 ## type(int fd, v3 ## type *ptr) \
	{ \
		READVEC(type, 3) \
		ptr->x = buf[0]; \
		ptr->y = buf[1]; \
		ptr->z = buf[2]; \
		return true; \
	} \
	bool write_v3 ## type(int fd, v3 ## type val) \
	{ \
		type vec[3] = {val.x, val.y, val.z}; \
		WRITEVEC(type, 3) \
	}

#define DEFTYP(type, bits) \
	bool read_ ## type(int fd, type *buf) \
	{ \
		u ## bits encoded; \
		int n_read; \
		if ((n_read = read(fd, &encoded, sizeof(encoded))) != sizeof(encoded)) { \
			if (n_read == -1) \
				perror("read"); \
			return false; \
		} \
		*buf = be ## bits ## toh(encoded); \
		return true; \
	} \
	bool write_ ## type(int fd, type val) \
	{ \
		u ## bits encoded = htobe ## bits(val); \
		if (write(fd, &encoded, sizeof(encoded)) == -1) { \
			perror("write"); \
			return false; \
		} \
		return true; \
	} \
	DEFVEC(type)

#define DEFTYPES(bits) \
	DEFTYP(s ## bits, bits) \
	DEFTYP(u ## bits, bits)

DEFTYPES(8)
DEFTYPES(16)
DEFTYPES(32)
DEFTYPES(64)