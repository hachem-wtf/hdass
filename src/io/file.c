#include <stdio.h>
#include <stdlib.h>

#include "diag/diag.h"
#include "io/file.h"

static void report_file_error(const char* verb, const char* path)
{
	char message[512];
	snprintf(message, sizeof(message), "%s '%s'", verb, path);
	report_error_message(message);
}

bool read_file(const char* path, struct File* out)
{
	FILE* stream = fopen(path, "rb");
	if (stream == NULL)
	{
		report_file_error("could not open", path);
		return false;
	}

	if (fseek(stream, 0, SEEK_END) != 0)
	{
		report_file_error("could not read", path);
		fclose(stream);
		return false;
	}

	long length = ftell(stream);
	if (length < 0)
	{
		report_file_error("could not read", path);
		fclose(stream);
		return false;
	}
	rewind(stream);

	size_t size = (size_t)length;
	char* data = malloc(size + 1);
	if (data == NULL)
	{
		report_file_error("out of memory reading", path);
		fclose(stream);
		return false;
	}

	if (fread(data, 1, size, stream) != size)
	{
		report_file_error("could not read", path);
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
