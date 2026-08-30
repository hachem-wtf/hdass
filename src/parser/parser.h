#pragma once

#include <stdbool.h>

#include "lexer/lexer.h"
#include "parser/ast.h"

bool parse_program(struct Lexer* lexer, struct Program* out);
