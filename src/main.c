#include <stdio.h>

#include "io/file.h"
#include "cli/args.h"
#include "lexer/lexer.h"
#include "codegen/nasm.h"
#include "parser/parser.h"

int main(int argc, char** argv)
{
	struct Args args;

	enum ParseResult result = parse_args(argc, argv, &args);
	if (result == PARSE_EXIT)
		return 0;
	if (result == PARSE_ERROR)
		return 1;

	if (args.target != ASSEMBLER_NASM)
	{
		fprintf(stderr, "error: only the nasm target is supported\n");
		return 1;
	}

	struct File source;
	if (!read_file(args.input_path, &source))
		return 1;

	struct Lexer lexer = create_lexer(source.data);

	struct Program program;
	if (!parse_program(&lexer, &program))
	{
		free_program(&program);
		free_file(&source);
		return 1;
	}

	FILE* out = stdout;
	if (args.output_path != NULL)
	{
		out = fopen(args.output_path, "w");
		if (out == NULL)
		{
			fprintf(stderr, "error: could not open '%s' for writing\n", args.output_path);
			free_program(&program);
			free_file(&source);
			return 1;
		}
	}

	generate_nasm(&program, out);

	if (out != stdout)
		fclose(out);

	free_program(&program);
	free_file(&source);
}
