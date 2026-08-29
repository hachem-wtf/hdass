#include <stdio.h>

#include "io/file.h"
#include "cli/args.h"

int main(int argc, char** argv)
{
	struct Args args;

	enum ParseResult result = parse_args(argc, argv, &args);
	if (result == PARSE_EXIT)
		return 0;
	if (result == PARSE_ERROR)
		return 1;

	struct File source;
	if (!read_file(args.input_path, &source))
		return 1;

	printf("read %zu bytes from %s\n", source.size, args.input_path);

	free_file(&source);
	return 0;
}
