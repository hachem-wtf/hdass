#pragma once

#include <stdbool.h>

#include "diag/diag.h"
#include "parser/ast.h"

bool analyze_program(struct Source source, struct Program* program);
