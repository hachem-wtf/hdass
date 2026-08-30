#pragma once

#include <stdio.h>

#include "parser/ast.h"

void generate_nasm(struct Program* program, FILE* out);
