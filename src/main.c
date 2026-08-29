#include <stdio.h>

#include "cli/args.h"

int main(int argc, char** argv)
{
	struct Args args;

	enum ParseResult result = parse_args(argc, argv, &args);
	if (result == PARSE_EXIT)
		return 0;
	if (result == PARSE_ERROR)
		return 1;

	static const char* target_names[] = { "nasm", "fasm", "masm" };

	printf("input:  %s\n", args.input_path);
	printf("output: %s\n", args.output_path != NULL ? args.output_path : "<stdout>");
	printf("target: %s\n", target_names[args.target]);

	return 0;
}
