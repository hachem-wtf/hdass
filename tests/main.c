#include <stdio.h>

#include "tests.h"

int main(void)
{
	struct TestContext context = { 0, 0 };

	run_lexer_tests(&context);
	run_parser_tests(&context);
	run_sema_tests(&context);
	run_codegen_tests(&context);

	printf("\n%d checks, %d failure(s)\n", context.checks, context.failures);
	return context.failures == 0 ? 0 : 1;
}
