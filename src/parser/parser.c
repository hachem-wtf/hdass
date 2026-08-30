#include <stdio.h>

#include "parser/parser.h"

struct Parser
{
	struct Lexer* lexer;
	struct Token current;
	struct Token previous;
	bool had_error;
};

static void advance_parser(struct Parser* parser)
{
	parser->previous = parser->current;
	parser->current = scan_token(parser->lexer);
}

static bool check(struct Parser* parser, enum TokenType type)
{
	return parser->current.type == type;
}

static void error_at(struct Parser* parser, struct Token token, const char* message)
{
	fprintf(stderr, "error: line %u: %s\n", token.line, message);
	parser->had_error = true;
}

static bool consume(struct Parser* parser, enum TokenType type, const char* message)
{
	if (check(parser, type))
	{
		advance_parser(parser);
		return true;
	}

	error_at(parser, parser->current, message);
	return false;
}

static bool parse_const(struct Parser* parser, struct Program* program)
{
	struct ConstDecl decl;

	if (!consume(parser, TOKEN_IDENTIFIER, "expected constant name after 'const'"))
		return false;
	decl.name = parser->previous;

	if (!consume(parser, TOKEN_EQUAL, "expected '=' after constant name"))
		return false;

	if (!consume(parser, TOKEN_INTEGER, "expected integer value after '='"))
		return false;
	decl.value = parser->previous;

	add_const(program, decl);
	return true;
}

static bool parse_data(struct Parser* parser, struct Program* program)
{
	struct DataDecl decl;

	if (!consume(parser, TOKEN_IDENTIFIER, "expected data name after 'data'"))
		return false;
	decl.name = parser->previous;

	if (!consume(parser, TOKEN_EQUAL, "expected '=' after data name"))
		return false;

	if (!consume(parser, TOKEN_STRING, "expected string value after '='"))
		return false;
	decl.value = parser->previous;

	add_data(program, decl);
	return true;
}

bool parse_program(struct Lexer* lexer, struct Program* out)
{
	struct Parser parser;
	parser.lexer = lexer;
	parser.had_error = false;
	advance_parser(&parser);

	*out = create_program();

	while (!check(&parser, TOKEN_EOF))
	{
		if (check(&parser, TOKEN_CONST))
		{
			advance_parser(&parser);
			if (!parse_const(&parser, out))
				return false;
		}
		else if (check(&parser, TOKEN_DATA))
		{
			advance_parser(&parser);
			if (!parse_data(&parser, out))
				return false;
		}
		else
		{
			error_at(&parser, parser.current, "expected a top-level declaration");
			return false;
		}
	}

	return !parser.had_error;
}
