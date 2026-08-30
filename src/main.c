#include <stdio.h>
#include <stddef.h>

#include "io/file.h"
#include "cli/args.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

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

	struct Lexer lexer = create_lexer(source.data);

	struct Program program;
	if (!parse_program(&lexer, &program))
	{
		free_program(&program);
		free_file(&source);
		return 1;
	}

	for (size_t i = 0; i < program.const_count; i += 1)
	{
		struct ConstDecl decl = program.consts[i];
		printf("const %.*s = %.*s\n",
			(int)decl.name.length, decl.name.start,
			(int)decl.value.length, decl.value.start);
	}

	free_program(&program);
	free_file(&source);
}
