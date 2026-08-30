#include <stdio.h>

#include "io/file.h"
#include "cli/args.h"
#include "lexer/lexer.h"

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

	for (;;)
	{
		struct Token token = scan_token(&lexer);
		printf("%4u  %-14s  %.*s\n", token.line, token_type_name(token.type), (int)token.length, token.start);
		if (token.type == TOKEN_EOF)
			break;
	}

	free_file(&source);
	return 0;
}
