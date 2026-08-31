#pragma once

#include "lexer/lexer.h"

struct Source
{
	const char* name;
	const char* text;
};

void report_error(struct Source source, struct Token token, const char* message);
void report_error_message(const char* message);
