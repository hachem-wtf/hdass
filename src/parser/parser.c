#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag/diag.h"
#include "parser/parser.h"

struct Parser
{
	struct Lexer* lexer;
	struct Source source;
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
	report_error(parser->source, token, message);
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

static struct Expr* parse_expression(struct Parser* parser);
static enum StoreSize parse_store_size(struct Parser* parser);

static bool parse_const(struct Parser* parser, struct Program* program)
{
	struct ConstDecl decl;

	if (!consume(parser, TOKEN_IDENTIFIER, "expected constant name after 'const'"))
		return false;
	decl.name = parser->previous;

	if (!consume(parser, TOKEN_EQUAL, "expected '=' after constant name"))
		return false;

	decl.value = parse_expression(parser);
	if (decl.value == NULL)
		return false;

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

static bool parse_enum(struct Parser* parser, struct Program* program)
{
	struct EnumDecl decl = create_enum();

	if (!consume(parser, TOKEN_IDENTIFIER, "expected enum name after 'enum'"))
		goto error;
	decl.name = parser->previous;

	if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' after enum name"))
		goto error;

	while (!check(parser, TOKEN_RIGHT_BRACE))
	{
		if (check(parser, TOKEN_EOF))
		{
			error_at(parser, parser->current, "unterminated enum");
			goto error;
		}

		if (!consume(parser, TOKEN_IDENTIFIER, "expected an enum member name"))
			goto error;
		add_enum_member(&decl, parser->previous);

		match_token(parser, TOKEN_COMMA);
	}
	advance_parser(parser);

	add_enum(program, decl);
	return true;

error:
	free(decl.members);
	return false;
}

static bool parse_struct(struct Parser* parser, struct Program* program)
{
	struct StructDecl decl = create_struct();

	if (!consume(parser, TOKEN_IDENTIFIER, "expected struct name after 'struct'"))
		goto error;
	decl.name = parser->previous;

	if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' after struct name"))
		goto error;

	while (!check(parser, TOKEN_RIGHT_BRACE))
	{
		if (check(parser, TOKEN_EOF))
		{
			error_at(parser, parser->current, "unterminated struct");
			goto error;
		}

		struct StructField field;
		if (!consume(parser, TOKEN_IDENTIFIER, "expected a field name"))
			goto error;
		field.name = parser->previous;

		field.size = STORE_SIZE_QWORD;
		if (match_token(parser, TOKEN_COLON))
		{
			field.size = parse_store_size(parser);
			if (field.size == STORE_SIZE_NONE)
			{
				error_at(parser, parser->current, "expected a size (byte, word, dword, qword) after ':'");
				goto error;
			}
		}

		add_struct_field(&decl, field);
		match_token(parser, TOKEN_COMMA);
	}
	advance_parser(parser);

	add_struct(program, decl);
	return true;

error:
	free(decl.fields);
	return false;
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

static bool is_assign_op(enum TokenType type)
{
	return type == TOKEN_EQUAL
		|| type == TOKEN_PLUS_EQUAL
		|| type == TOKEN_MINUS_EQUAL
		|| type == TOKEN_STAR_EQUAL
		|| type == TOKEN_SLASH_EQUAL;
}

static struct Expr* alloc_expr(enum ExprKind kind)
{
	struct Expr* expr = malloc(sizeof(struct Expr));
	expr->kind = kind;
	return expr;
}

static enum StoreSize parse_store_size(struct Parser* parser)
{
	if (match_token(parser, TOKEN_BYTE))
		return STORE_SIZE_BYTE;
	if (match_token(parser, TOKEN_WORD))
		return STORE_SIZE_WORD;
	if (match_token(parser, TOKEN_DWORD))
		return STORE_SIZE_DWORD;
	if (match_token(parser, TOKEN_QWORD))
		return STORE_SIZE_QWORD;
	return STORE_SIZE_NONE;
}

static struct Expr* parse_primary(struct Parser* parser)
{
	if (match_token(parser, TOKEN_CARET))
	{
		enum StoreSize size = parse_store_size(parser);

		struct Expr* address = parse_primary(parser);
		if (address == NULL)
			return NULL;

		struct Expr* deref = alloc_expr(EXPR_DEREF);
		deref->deref.size = size;
		deref->deref.address = address;
		return deref;
	}

	if (check(parser, TOKEN_IDENTIFIER) || check(parser, TOKEN_INTEGER)
		|| check(parser, TOKEN_FLOAT) || check(parser, TOKEN_CHAR))
	{
		advance_parser(parser);

		struct Expr* expr = alloc_expr(EXPR_PRIMARY);
		expr->primary.token = parser->previous;
		return expr;
	}

	error_at(parser, parser->current, "expected an expression");
	return NULL;
}

static struct Expr* parse_postfix(struct Parser* parser)
{
	struct Expr* expr = parse_primary(parser);
	if (expr == NULL)
		return NULL;

	while (match_token(parser, TOKEN_DOT))
	{
		// an identifier is a member (data.len); an integer is a register size
		// suffix (r1.64), meaningful with the logical_registers extension
		if (!check(parser, TOKEN_IDENTIFIER) && !check(parser, TOKEN_INTEGER))
		{
			error_at(parser, parser->current, "expected a member name or size after '.'");
			free_expr(expr);
			return NULL;
		}
		advance_parser(parser);

		struct Expr* member = alloc_expr(EXPR_MEMBER);
		member->member.object = expr;
		member->member.member = parser->previous;
		expr = member;
	}

	return expr;
}

static struct Expr* parse_binary(struct Parser* parser, struct Expr* (*operand)(struct Parser*), enum TokenType a, enum TokenType b)
{
	struct Expr* left = operand(parser);
	if (left == NULL)
		return NULL;

	while (check(parser, a) || check(parser, b))
	{
		advance_parser(parser);
		struct Token op = parser->previous;

		struct Expr* right = operand(parser);
		if (right == NULL)
		{
			free_expr(left);
			return NULL;
		}

		struct Expr* binary = alloc_expr(EXPR_BINARY);
		binary->binary.left = left;
		binary->binary.op = op;
		binary->binary.right = right;
		left = binary;
	}

	return left;
}

static struct Expr* parse_multiplicative(struct Parser* parser)
{
	return parse_binary(parser, parse_postfix, TOKEN_STAR, TOKEN_SLASH);
}

static struct Expr* parse_expression(struct Parser* parser)
{
	return parse_binary(parser, parse_multiplicative, TOKEN_PLUS, TOKEN_MINUS);
}

static bool is_compare_op(enum TokenType type)
{
	return type == TOKEN_EQUAL_EQUAL
		|| type == TOKEN_BANG_EQUAL
		|| type == TOKEN_LESS
		|| type == TOKEN_LESS_EQUAL
		|| type == TOKEN_GREATER
		|| type == TOKEN_GREATER_EQUAL;
}

static bool parse_call(struct Parser* parser, struct Token name, struct Statement* out)
{
	struct Expr** args = NULL;
	size_t count = 0;
	size_t capacity = 0;

	if (!check(parser, TOKEN_RIGHT_PAREN))
	{
		do
		{
			struct Expr* arg = parse_expression(parser);
			if (arg == NULL)
				goto error;

			if (count == capacity)
			{
				capacity = capacity < 4 ? 4 : capacity * 2;
				args = realloc(args, capacity * sizeof(struct Expr*));
			}
			args[count] = arg;
			count += 1;
		}
		while (match_token(parser, TOKEN_COMMA));
	}

	if (!consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after arguments"))
		goto error;

	out->kind = STATEMENT_CALL;
	out->call.name = name;
	out->call.args = args;
	out->call.arg_count = count;
	out->call.arg_capacity = capacity;
	return true;

error:
	for (size_t i = 0; i < count; i += 1)
		free_expr(args[i]);
	free(args);
	return false;
}

static bool parse_statement(struct Parser* parser, struct Statement* out);

static bool parse_if(struct Parser* parser, struct Statement* out)
{
	struct Expr* left = parse_expression(parser);
	if (left == NULL)
		return false;

	if (!is_compare_op(parser->current.type))
	{
		error_at(parser, parser->current, "expected a comparison operator");
		free_expr(left);
		return false;
	}
	advance_parser(parser);
	struct Token comparison = parser->previous;

	struct Expr* right = parse_expression(parser);
	if (right == NULL)
	{
		free_expr(left);
		return false;
	}

	struct Statement* body = malloc(sizeof(struct Statement));
	if (!parse_statement(parser, body))
	{
		free(body);
		free_expr(left);
		free_expr(right);
		return false;
	}

	out->kind = STATEMENT_IF;
	out->branch.left = left;
	out->branch.comparison = comparison;
	out->branch.right = right;
	out->branch.body = body;
	return true;
}

static bool parse_statement(struct Parser* parser, struct Statement* out)
{
	if (match_token(parser, TOKEN_IF))
		return parse_if(parser, out);

	if (match_token(parser, TOKEN_SYSCALL))
	{
		out->kind = STATEMENT_SYSCALL;
		return true;
	}

	if (match_token(parser, TOKEN_STACK))
	{
		if (!consume(parser, TOKEN_IDENTIFIER, "expected buffer name after 'stack'"))
			return false;
		struct Token name = parser->previous;

		if (!consume(parser, TOKEN_LEFT_BRACKET, "expected '[' after buffer name"))
			return false;

		struct Expr* size = parse_expression(parser);
		if (size == NULL)
			return false;

		if (!consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after buffer size"))
		{
			free_expr(size);
			return false;
		}

		out->kind = STATEMENT_STACK;
		out->stack.name = name;
		out->stack.size = size;
		return true;
	}

	if (match_token(parser, TOKEN_GOTO))
	{
		if (!consume(parser, TOKEN_IDENTIFIER, "expected label after 'goto'"))
			return false;

		out->kind = STATEMENT_GOTO;
		out->jump.label = parser->previous;
		return true;
	}

	bool deref = match_token(parser, TOKEN_CARET);
	enum StoreSize store_size = deref ? parse_store_size(parser) : STORE_SIZE_NONE;

	if (!consume(parser, TOKEN_IDENTIFIER, "expected a statement"))
		return false;
	struct Token name = parser->previous;

	if (!deref && match_token(parser, TOKEN_LEFT_PAREN))
		return parse_call(parser, name, out);

	if (!deref && match_token(parser, TOKEN_COLON))
	{
		out->kind = STATEMENT_LABEL;
		out->label.name = name;
		return true;
	}

	if (!is_assign_op(parser->current.type))
	{
		error_at(parser, parser->current, "expected an assignment operator");
		return false;
	}

	advance_parser(parser);
	struct Token op = parser->previous;

	struct Expr* value = parse_expression(parser);
	if (value == NULL)
		return false;

	out->kind = STATEMENT_ASSIGN;
	out->assign.target_deref = deref;
	out->assign.store_size = store_size;
	out->assign.target = name;
	out->assign.op = op;
	out->assign.value = value;
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

	while (!check(parser, TOKEN_RIGHT_BRACE))
	{
		if (check(parser, TOKEN_EOF))
		{
			error_at(parser, parser->current, "unterminated procedure body");
			goto error;
		}

		struct Statement statement;
		if (!parse_statement(parser, &statement))
			goto error;

		add_statement(&decl, statement);
	}
	advance_parser(parser);

	add_proc(program, decl);
	return true;

error:
	free_proc(&decl);
	return false;
}

static bool token_text_is(struct Token token, const char* text)
{
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static bool parse_directive(struct Parser* parser, struct Program* program)
{
	if (!consume(parser, TOKEN_IDENTIFIER, "expected directive name after '['"))
		return false;
	struct Token key = parser->previous;

	if (!consume(parser, TOKEN_COLON, "expected ':' after directive name"))
		return false;

	if (!check(parser, TOKEN_IDENTIFIER) && !check(parser, TOKEN_INTEGER))
	{
		error_at(parser, parser->current, "expected a directive value");
		return false;
	}
	advance_parser(parser);
	struct Token value = parser->previous;

	if (!consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' to close directive"))
		return false;

	if (token_text_is(key, "bits"))
	{
		if (value.type != TOKEN_INTEGER || (!token_text_is(value, "64") && !token_text_is(value, "32")))
		{
			error_at(parser, value, "bits must be 32 or 64");
			return false;
		}
		program->config.bits = token_text_is(value, "64") ? 64 : 32;
		return true;
	}

	if (token_text_is(key, "entry"))
	{
		if (value.type != TOKEN_IDENTIFIER)
		{
			error_at(parser, value, "entry must be a procedure name");
			return false;
		}
		program->config.has_entry = true;
		program->config.entry = value;
		return true;
	}

	if (token_text_is(key, "enable"))
	{
		if (value.type == TOKEN_IDENTIFIER && token_text_is(value, "logical_registers"))
		{
			program->config.logical_registers = true;
			return true;
		}
		error_at(parser, value, "unknown extension");
		return false;
	}

	error_at(parser, key, "unknown directive");
	return false;
}

bool parse_program(struct Lexer* lexer, struct Program* out)
{
	struct Parser parser;
	parser.lexer = lexer;
	parser.source.name = lexer->name;
	parser.source.text = lexer->source;
	parser.had_error = false;
	advance_parser(&parser);

	*out = create_program();

	while (!check(&parser, TOKEN_EOF))
	{
		if (check(&parser, TOKEN_LEFT_BRACKET))
		{
			advance_parser(&parser);
			if (!parse_directive(&parser, out))
				return false;
		}
		else if (check(&parser, TOKEN_CONST))
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
		else if (check(&parser, TOKEN_ENUM))
		{
			advance_parser(&parser);
			if (!parse_enum(&parser, out))
				return false;
		}
		else if (check(&parser, TOKEN_STRUCT))
		{
			advance_parser(&parser);
			if (!parse_struct(&parser, out))
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
