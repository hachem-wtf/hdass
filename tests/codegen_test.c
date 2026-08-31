#include <stdio.h>
#include <string.h>

#include "parser/parser.h"
#include "codegen/nasm.h"
#include "tests.h"

static void test_generate_consts_and_data(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const N = 5\ndata msg = \"hi\"\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "%define N 5") != NULL);
	check(context, strstr(buffer, "section .data") != NULL);
	check(context, strstr(buffer, "msg: db `hi`") != NULL);
	check(context, strstr(buffer, ".len equ $ - msg") != NULL);
	check(context, strstr(buffer, "section .text") != NULL);

	free_program(&program);
}

static void test_generate_text(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nrax = SYS_WRITE\nrsi = message\nrdx = message.len\n^rdi = rax\nloop:\nsyscall\ngoto loop\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "main:") != NULL);
	check(context, strstr(buffer, "mov rax, SYS_WRITE") != NULL);
	check(context, strstr(buffer, "mov rsi, message") != NULL);
	check(context, strstr(buffer, "mov rdx, message.len") != NULL);
	check(context, strstr(buffer, "mov [rdi], rax") != NULL);
	check(context, strstr(buffer, "loop:") != NULL);
	check(context, strstr(buffer, "syscall") != NULL);
	check(context, strstr(buffer, "jmp loop") != NULL);

	free_program(&program);
}

static void test_generate_if(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nif rcx != 0\ngoto loop\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "cmp rcx, 0") != NULL);
	check(context, strstr(buffer, "je .if_end_0") != NULL);
	check(context, strstr(buffer, "jmp loop") != NULL);
	check(context, strstr(buffer, ".if_end_0:") != NULL);

	free_program(&program);
}

static void test_generate_call(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc print_number(value: rdi)\n{\nsyscall\n}\nproc main\n{\nprint_number(r12)\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rdi, r12") != NULL);
	check(context, strstr(buffer, "call print_number") != NULL);
	check(context, strstr(buffer, "\tret\n") != NULL);

	free_program(&program);
}

static void test_generate_param_substitution(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc print_number(value: rdi)\n{\nrax = value\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, rdi") != NULL);
	check(context, strstr(buffer, "value") == NULL);

	free_program(&program);
}

static void test_generate_divide(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax /= rbx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "cqo") != NULL);
	check(context, strstr(buffer, "idiv rbx") != NULL);

	free_program(&program);
}

static void test_generate_multiply(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax = rbx * rcx\nrbx = rcx * 3 + rdx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, rbx") != NULL);
	check(context, strstr(buffer, "imul rax, rcx") != NULL);
	check(context, strstr(buffer, "imul rbx, 3") != NULL);
	check(context, strstr(buffer, "add rbx, rdx") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_divide_nonrax(struct TestContext* context)
{
	// division into a register other than rax routes through rax:rdx
	struct Lexer lexer = create_lexer("proc main\n{\nrbx /= rcx\nrsi = rdi / rcx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, rbx") != NULL);
	check(context, strstr(buffer, "idiv rcx") != NULL);
	check(context, strstr(buffer, "mov rbx, rax") != NULL);
	check(context, strstr(buffer, "mov rsi, rax") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_stack_frame(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc work\n{\nstack buffer[32]\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "push rbp") != NULL);
	check(context, strstr(buffer, "mov rbp, rsp") != NULL);
	check(context, strstr(buffer, "sub rsp, 32") != NULL);
	check(context, strstr(buffer, "leave") != NULL);

	free_program(&program);
}

static void test_generate_address_expr(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc work\n{\nstack buffer[32]\nrsi = buffer + 31\nrdx = buffer + 32 - rsi\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "lea rsi, [rbp - 32]") != NULL);
	check(context, strstr(buffer, "add rsi, 31") != NULL);
	check(context, strstr(buffer, "lea rdx, [rbp - 32]") != NULL);
	check(context, strstr(buffer, "add rdx, 32") != NULL);
	check(context, strstr(buffer, "sub rdx, rsi") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_sized_store(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\n^byte rsi = rdx\n^dword rsi = rax\n^byte rsi = 5\n^rsi = rbx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov byte [rsi], dl") != NULL);
	check(context, strstr(buffer, "mov dword [rsi], eax") != NULL);
	check(context, strstr(buffer, "mov byte [rsi], 5") != NULL);
	check(context, strstr(buffer, "mov [rsi], rbx") != NULL);

	free_program(&program);
}

static void test_generate_entry_and_bits(struct TestContext* context)
{
	struct Lexer with_entry = create_lexer("[bits: 64]\n[entry: main]\nproc main\n{\nsyscall\n}\nproc helper\n{\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&with_entry, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "bits 64") != NULL);
	check(context, strstr(buffer, "global main") != NULL);
	check(context, strstr(buffer, "main:") != NULL);
	check(context, strstr(buffer, "_start") == NULL);
	// the entry proc does not return; the non-entry helper does
	check(context, strstr(buffer, "helper:\n\tsyscall\n\tret") != NULL);

	free_program(&program);
}

static void test_generate_no_entry(struct TestContext* context)
{
	struct Lexer no_entry = create_lexer("proc main\n{\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&no_entry, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "global") == NULL);
	check(context, strstr(buffer, "main:") != NULL);

	free_program(&program);
}

static void test_generate_logical_registers(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"[enable: logical_registers]\nproc main\n{\nr1 = 5\nr4 = r10\nr6 = r1.8\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, 5") != NULL);    // r1 -> rax
	check(context, strstr(buffer, "mov rdx, r11") != NULL);  // r4 -> rdx, r10 -> r11
	check(context, strstr(buffer, "mov rdi, al") != NULL);   // r6 -> rdi, r1.8 -> al

	free_program(&program);
}

static void test_generate_logical_disabled(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nr1 = 5\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	// without the extension, r1 is passed through untouched
	check(context, strstr(buffer, "mov r1, 5") != NULL);

	free_program(&program);
}

static void test_generate_load(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nrax = ^rsi\nrbx = ^byte rsi\nrcx = ^dword rsi\nrdx = ^rsi + 4\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, [rsi]") != NULL);
	check(context, strstr(buffer, "movzx rbx, byte [rsi]") != NULL);
	check(context, strstr(buffer, "mov ecx, [rsi]") != NULL);
	check(context, strstr(buffer, "mov rdx, [rsi]") != NULL);
	check(context, strstr(buffer, "add rdx, 4") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

void run_codegen_tests(struct TestContext* context)
{
	test_generate_load(context);
	test_generate_logical_registers(context);
	test_generate_logical_disabled(context);
	test_generate_entry_and_bits(context);
	test_generate_no_entry(context);
	test_generate_consts_and_data(context);
	test_generate_text(context);
	test_generate_if(context);
	test_generate_call(context);
	test_generate_param_substitution(context);
	test_generate_divide(context);
	test_generate_multiply(context);
	test_generate_divide_nonrax(context);
	test_generate_stack_frame(context);
	test_generate_address_expr(context);
	test_generate_sized_store(context);
}
