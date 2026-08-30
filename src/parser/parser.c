#include <stdio.h>
#include <stdlib.h>

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

static bool match_token(struct Parser* parser, enum TokenType type)
{
	if (!check(parser, type))
		return false;

	advance_parser(parser);
	return true;
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

static bool parse_params(struct Parser* parser, struct ProcDecl* proc)
{
	if (check(parser, TOKEN_RIGHT_PAREN))
		return true;

	do
	{
		struct Param param;

		if (!consume(parser, TOKEN_IDENTIFIER, "expected parameter name"))
			return false;
		param.name = parser->previous;

		if (!consume(parser, TOKEN_COLON, "expected ':' after parameter name"))
			return false;

		if (!consume(parser, TOKEN_IDENTIFIER, "expected register after ':'"))
			return false;
		param.reg = parser->previous;

		add_param(proc, param);
	}
	while (match_token(parser, TOKEN_COMMA));

	return true;
}

static bool parse_proc(struct Parser* parser, struct Program* program)
{
	struct ProcDecl decl = create_proc();

	if (!consume(parser, TOKEN_IDENTIFIER, "expected procedure name after 'proc'"))
		goto error;
	decl.name = parser->previous;

	if (match_token(parser, TOKEN_LEFT_PAREN))
	{
		if (!parse_params(parser, &decl))
			goto error;
		if (!consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after parameters"))
			goto error;
	}

	if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' to begin procedure body"))
		goto error;

	// TODO: parse body statements; for now skip the braced block
	int depth = 1;
	while (depth > 0)
	{
		if (check(parser, TOKEN_EOF))
		{
			error_at(parser, parser->current, "unterminated procedure body");
			goto error;
		}
		if (check(parser, TOKEN_LEFT_BRACE))
			depth += 1;
		else if (check(parser, TOKEN_RIGHT_BRACE))
			depth -= 1;
		advance_parser(parser);
	}

	add_proc(program, decl);
	return true;

error:
	free(decl.params);
	return false;
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
		else if (check(&parser, TOKEN_PROC))
		{
			advance_parser(&parser);
			if (!parse_proc(&parser, out))
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
