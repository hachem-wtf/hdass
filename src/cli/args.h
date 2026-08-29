#pragma once

enum Assembler
{
	ASSEMBLER_NASM,
	ASSEMBLER_FASM,
	ASSEMBLER_MASM,
};

enum ParseResult
{
	PARSE_OK,
	PARSE_EXIT,
	PARSE_ERROR,
};

struct Args
{
	const char* input_path;
	const char* output_path;
	enum Assembler target;
};

enum ParseResult parse_args(int argc, char** argv, struct Args* args);
void print_usage(const char* program);
void print_version(void);
