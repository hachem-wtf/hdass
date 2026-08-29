#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "cli/args.h"

#define HDASS_VERSION "0.1.0"

static bool match_assembler(const char* name, enum Assembler* out)
{
	if (strcmp(name, "nasm") == 0)
	{
		*out = ASSEMBLER_NASM;
		return true;
	}
	if (strcmp(name, "fasm") == 0)
	{
		*out = ASSEMBLER_FASM;
		return true;
	}
	if (strcmp(name, "masm") == 0)
	{
		*out = ASSEMBLER_MASM;
		return true;
	}
	return false;
}

void print_version(void)
{
	printf("hdass %s\n", HDASS_VERSION);
}

void print_usage(const char* program)
{
	printf("hachem's dumb assembly super set\n\n");
	printf("usage: %s <input.hdass> [options]\n\n", program);
	printf("options:\n");
	printf("  -o, --output <file>   write output to <file> (default: stdout)\n");
	printf("  -t, --target <name>   target assembler: nasm, fasm, masm (default: nasm)\n");
	printf("  -h, --help            print this help and exit\n");
	printf("  -v, --version         print version and exit\n");
}

enum ParseResult parse_args(int argc, char** argv, struct Args* args)
{
	const char* program = argv[0];

	args->input_path = NULL;
	args->output_path = NULL;
	args->target = ASSEMBLER_NASM;

	for (int i = 1; i < argc; i += 1)
	{
		const char* arg = argv[i];

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
		{
			print_usage(program);
			return PARSE_EXIT;
		}

		if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0)
		{
			print_version();
			return PARSE_EXIT;
		}

		if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0)
		{
			i += 1;
			if (i >= argc)
			{
				fprintf(stderr, "error: '%s' requires an argument\n", arg);
				return PARSE_ERROR;
			}
			args->output_path = argv[i];
			continue;
		}

		if (strcmp(arg, "-t") == 0 || strcmp(arg, "--target") == 0)
		{
			i += 1;
			if (i >= argc)
			{
				fprintf(stderr, "error: '%s' requires an argument\n", arg);
				return PARSE_ERROR;
			}
			if (!match_assembler(argv[i], &args->target))
			{
				fprintf(stderr, "error: unknown target '%s'\n", argv[i]);
				return PARSE_ERROR;
			}
			continue;
		}

		if (arg[0] == '-')
		{
			fprintf(stderr, "error: unknown option '%s'\n", arg);
			return PARSE_ERROR;
		}

		if (args->input_path != NULL)
		{
			fprintf(stderr, "error: multiple input files given ('%s' and '%s')\n", args->input_path, arg);
			return PARSE_ERROR;
		}

		args->input_path = arg;
	}

	if (args->input_path == NULL)
	{
		fprintf(stderr, "error: no input file given\n");
		return PARSE_ERROR;
	}

	return PARSE_OK;
}
