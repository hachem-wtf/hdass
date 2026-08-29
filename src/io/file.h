#pragma once

#include <stddef.h>
#include <stdbool.h>

struct File
{
	char* data;
	size_t size;
};

bool read_file(const char* path, struct File* out);
void free_file(struct File* file);
