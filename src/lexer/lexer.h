#pragma once

#include <stddef.h>
#include <stdint.h>

enum TokenType
{
	TOKEN_EOF,
	TOKEN_IDENTIFIER,
	TOKEN_INTEGER,
	TOKEN_FLOAT,
	TOKEN_STRING,
	TOKEN_CHAR,

	TOKEN_CONST,
	TOKEN_DATA,
	TOKEN_PROC,
	TOKEN_ENUM,
	TOKEN_STRUCT,
	TOKEN_STACK,
	TOKEN_IF,
	TOKEN_GOTO,
	TOKEN_SYSCALL,
	TOKEN_BYTE,
	TOKEN_WORD,
	TOKEN_DWORD,
	TOKEN_QWORD,

	TOKEN_EQUAL,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_STAR,
	TOKEN_SLASH,
	TOKEN_PERCENT,
	TOKEN_CARET,
	TOKEN_BANG,
	TOKEN_LESS,
	TOKEN_GREATER,
	TOKEN_DOT,
	TOKEN_COMMA,
	TOKEN_COLON,
	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACKET,
	TOKEN_RIGHT_BRACKET,
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,

	TOKEN_EQUAL_EQUAL,
	TOKEN_BANG_EQUAL,
	TOKEN_PLUS_EQUAL,
	TOKEN_MINUS_EQUAL,
	TOKEN_STAR_EQUAL,
	TOKEN_SLASH_EQUAL,
	TOKEN_PERCENT_EQUAL,
	TOKEN_LESS_EQUAL,
	TOKEN_GREATER_EQUAL,

	TOKEN_UNKNOWN,
};

struct Token
{
	enum TokenType type;
	const char* start;
	size_t length;
	uint32_t line;
};

struct Lexer
{
	const char* name;
	const char* source;
	const char* current;
	uint32_t line;
};

struct Lexer create_lexer(const char* source);
struct Token scan_token(struct Lexer* lexer);
const char* token_type_name(enum TokenType type);
