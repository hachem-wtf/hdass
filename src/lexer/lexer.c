#include <string.h>
#include <stdbool.h>

#include "lexer/lexer.h"

static bool is_alpha(char character)
{
	return (character >= 'a' && character <= 'z') || 
		   (character >= 'A' && character <= 'Z') || 
		    character == '_';
}

static bool is_digit(char character)
{
	return character >= '0' && character <= '9';
}

static bool is_hex_digit(char character)
{
	return is_digit(character)
		|| (character >= 'a' && character <= 'f')
		|| (character >= 'A' && character <= 'F');
}

static enum TokenType identifier_type(const char* start, size_t length)
{
	static const struct Keyword
	{
		const char* text;
		size_t length;
		enum TokenType type;
	} keywords[] = 
	{
		{ "const",   5, TOKEN_CONST   },
		{ "data",    4, TOKEN_DATA    },
		{ "proc",    4, TOKEN_PROC    },
		{ "enum",    4, TOKEN_ENUM    },
		{ "struct",  6, TOKEN_STRUCT  },
		{ "stack",   5, TOKEN_STACK   },
		{ "if",      2, TOKEN_IF      },
		{ "goto",    4, TOKEN_GOTO    },
		{ "syscall", 7, TOKEN_SYSCALL },
		{ "byte",    4, TOKEN_BYTE    },
		{ "word",    4, TOKEN_WORD    },
		{ "dword",   5, TOKEN_DWORD   },
		{ "qword",   5, TOKEN_QWORD   },
	};

	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i += 1)
	{
		const struct Keyword* keyword = &keywords[i];
		if (keyword->length == length && memcmp(start, keyword->text, length) == 0)
			return keyword->type;
	}

	return TOKEN_IDENTIFIER;
}

static char peek(struct Lexer* lexer)
{
	return *lexer->current;
}

static char advance(struct Lexer* lexer)
{
	char c = *lexer->current;
	lexer->current += 1;
	return c;
}

static bool match(struct Lexer* lexer, char expected)
{
	if (*lexer->current != expected)
		return false;
	lexer->current += 1;
	return true;
}

static void skip_whitespace(struct Lexer* lexer)
{
	for (;;)
	{
		char c = peek(lexer);
		if (c == ' ' || c == '\t' || c == '\r')
			advance(lexer);
		else if (c == '\n')
		{
			lexer->line += 1;
			advance(lexer);
		}
		else if (c == '/' && lexer->current[1] == '/')
		{
			while (peek(lexer) != '\n' && peek(lexer) != '\0')
				advance(lexer);
		}
		else if (c == '/' && lexer->current[1] == '*')
		{
			advance(lexer);
			advance(lexer);
			while (!(peek(lexer) == '*' && lexer->current[1] == '/') && peek(lexer) != '\0')
			{
				if (peek(lexer) == '\n')
					lexer->line += 1;
				advance(lexer);
			}
			if (peek(lexer) != '\0')
			{
				advance(lexer);
				advance(lexer);
			}
		}
		else
			return;
	}
}

static struct Token make_token(struct Lexer* lexer, enum TokenType type, const char* start)
{
	struct Token token;
	token.type = type;
	token.start = start;
	token.length = (size_t)(lexer->current - start);
	token.line = lexer->line;
	return token;
}

struct Lexer create_lexer(const char* source)
{
	struct Lexer lexer;
	lexer.name = NULL;
	lexer.source = source;
	lexer.current = source;
	lexer.line = 1;
	return lexer;
}

static struct Token scan_string(struct Lexer* lexer, const char* start)
{
	while (peek(lexer) != '"')
	{
		char character = peek(lexer);
		if (character == '\0')
			return make_token(lexer, TOKEN_UNKNOWN, start);
		if (character == '\n')
			lexer->line += 1;
		if (character == '\\' && lexer->current[1] != '\0')
			advance(lexer);
		advance(lexer);
	}

	advance(lexer);
	return make_token(lexer, TOKEN_STRING, start);
}

static struct Token scan_char(struct Lexer* lexer, const char* start)
{
	while (peek(lexer) != '\'')
	{
		char character = peek(lexer);
		if (character == '\0' || character == '\n')
			return make_token(lexer, TOKEN_UNKNOWN, start);
		if (character == '\\' && lexer->current[1] != '\0')
			advance(lexer);
		advance(lexer);
	}

	advance(lexer);
	return make_token(lexer, TOKEN_CHAR, start);
}

struct Token scan_token(struct Lexer* lexer)
{
	skip_whitespace(lexer);

	const char* start = lexer->current;
	if (peek(lexer) == '\0')
		return make_token(lexer, TOKEN_EOF, start);
	char character = advance(lexer);

	if (is_alpha(character))
	{
		while (is_alpha(peek(lexer)) || is_digit(peek(lexer)))
			advance(lexer);
		return make_token(lexer, identifier_type(start, (size_t)(lexer->current - start)), start);
	}

	if (is_digit(character))
	{
		if (character == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X'))
		{
			advance(lexer);
			while (is_hex_digit(peek(lexer)))
				advance(lexer);
			return make_token(lexer, TOKEN_INTEGER, start);
		}

		if (character == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B'))
		{
			advance(lexer);
			while (peek(lexer) == '0' || peek(lexer) == '1')
				advance(lexer);
			return make_token(lexer, TOKEN_INTEGER, start);
		}

		while (is_digit(peek(lexer)))
			advance(lexer);

		if (peek(lexer) == '.' && is_digit(lexer->current[1]))
		{
			advance(lexer);
			while (is_digit(peek(lexer)))
				advance(lexer);
			return make_token(lexer, TOKEN_FLOAT, start);
		}

		return make_token(lexer, TOKEN_INTEGER, start);
	}

	if (character == '"')
		return scan_string(lexer, start);
	if (character == '\'')
		return scan_char(lexer, start);

	switch (character)
	{
		case '=': return make_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL   : TOKEN_EQUAL,   start);
		case '!': return make_token(lexer, match(lexer, '=') ? TOKEN_BANG_EQUAL    : TOKEN_BANG,    start);
		case '<': return make_token(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL    : TOKEN_LESS,    start);
		case '>': return make_token(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER, start);
		case '+': return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQUAL    : TOKEN_PLUS,    start);
		case '-': return make_token(lexer, match(lexer, '=') ? TOKEN_MINUS_EQUAL   : TOKEN_MINUS,   start);
		case '*': return make_token(lexer, match(lexer, '=') ? TOKEN_STAR_EQUAL    : TOKEN_STAR,    start);
		case '/': return make_token(lexer, match(lexer, '=') ? TOKEN_SLASH_EQUAL   : TOKEN_SLASH,   start);

		case '^': return make_token(lexer, TOKEN_CARET,         start);
		case '.': return make_token(lexer, TOKEN_DOT,           start);
		case ',': return make_token(lexer, TOKEN_COMMA,         start);
		case ':': return make_token(lexer, TOKEN_COLON,         start);
		case '(': return make_token(lexer, TOKEN_LEFT_PAREN,    start);
		case ')': return make_token(lexer, TOKEN_RIGHT_PAREN,   start);
		case '[': return make_token(lexer, TOKEN_LEFT_BRACKET,  start);
		case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET, start);
		case '{': return make_token(lexer, TOKEN_LEFT_BRACE,    start);
		case '}': return make_token(lexer, TOKEN_RIGHT_BRACE,   start);
	}

	return make_token(lexer, TOKEN_UNKNOWN, start);
}

const char* token_type_name(enum TokenType type)
{
	switch (type)
	{
		case TOKEN_EOF:           return "eof";
		case TOKEN_IDENTIFIER:    return "identifier";
		case TOKEN_INTEGER:       return "integer";
		case TOKEN_FLOAT:         return "float";
		case TOKEN_STRING:        return "string";
		case TOKEN_CHAR:          return "char";
		case TOKEN_CONST:         return "const";
		case TOKEN_DATA:          return "data";
		case TOKEN_PROC:          return "proc";
		case TOKEN_ENUM:          return "enum";
		case TOKEN_STRUCT:        return "struct";
		case TOKEN_STACK:         return "stack";
		case TOKEN_IF:            return "if";
		case TOKEN_GOTO:          return "goto";
		case TOKEN_SYSCALL:       return "syscall";
		case TOKEN_BYTE:          return "byte";
		case TOKEN_WORD:          return "word";
		case TOKEN_DWORD:         return "dword";
		case TOKEN_QWORD:         return "qword";
		case TOKEN_EQUAL:         return "equal";
		case TOKEN_PLUS:          return "plus";
		case TOKEN_MINUS:         return "minus";
		case TOKEN_STAR:          return "star";
		case TOKEN_SLASH:         return "slash";
		case TOKEN_CARET:         return "caret";
		case TOKEN_BANG:          return "bang";
		case TOKEN_LESS:          return "less";
		case TOKEN_GREATER:       return "greater";
		case TOKEN_DOT:           return "dot";
		case TOKEN_COMMA:         return "comma";
		case TOKEN_COLON:         return "colon";
		case TOKEN_LEFT_PAREN:    return "left_paren";
		case TOKEN_RIGHT_PAREN:   return "right_paren";
		case TOKEN_LEFT_BRACKET:  return "left_bracket";
		case TOKEN_RIGHT_BRACKET: return "right_bracket";
		case TOKEN_LEFT_BRACE:    return "left_brace";
		case TOKEN_RIGHT_BRACE:   return "right_brace";
		case TOKEN_EQUAL_EQUAL:   return "equal_equal";
		case TOKEN_BANG_EQUAL:    return "bang_equal";
		case TOKEN_PLUS_EQUAL:    return "plus_equal";
		case TOKEN_MINUS_EQUAL:   return "minus_equal";
		case TOKEN_STAR_EQUAL:    return "star_equal";
		case TOKEN_SLASH_EQUAL:   return "slash_equal";
		case TOKEN_LESS_EQUAL:    return "less_equal";
		case TOKEN_GREATER_EQUAL: return "greater_equal";
		case TOKEN_UNKNOWN:       return "unknown";
	}

	return "unknown";
}

