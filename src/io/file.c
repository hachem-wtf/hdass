#include <stdio.h>
#include <stdlib.h>

#include "io/file.h"

bool read_file(const char* path, struct File* out)
{
	FILE* stream = fopen(path, "rb");
	if (stream == NULL)
	{
		fprintf(stderr, "error: could not open '%s'\n", path);
		return false;
	}

	if (fseek(stream, 0, SEEK_END) != 0)
	{
		fprintf(stderr, "error: could not read '%s'\n", path);
		fclose(stream);
		return false;
	}

	long length = ftell(stream);
	if (length < 0)
	{
		fprintf(stderr, "error: could not read '%s'\n", path);
		fclose(stream);
		return false;
	}
	rewind(stream);

	size_t size = (size_t)length;
	char* data = malloc(size + 1);
	if (data == NULL)
	{
		fprintf(stderr, "error: out of memory reading '%s'\n", path);
		fclose(stream);
		return false;
	}

	if (fread(data, 1, size, stream) != size)
	{
		fprintf(stderr, "error: could not read '%s'\n", path);
		free(data);
		fclose(stream);
		return false;
	}

	data[size] = '\0';
	fclose(stream);

	out->data = data;
	out->size = size;
	return true;
}

void free_file(struct File* file)
{
	free(file->data);
	file->data = NULL;
	file->size = 0;
}
