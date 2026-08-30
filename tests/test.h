#pragma once

#include <stdio.h>

struct TestContext
{
	int checks;
	int failures;
};

#define check(context, condition) \
	do \
	{ \
		(context)->checks += 1; \
		if (!(condition)) \
		{ \
			(context)->failures += 1; \
			printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} \
	while (0)
