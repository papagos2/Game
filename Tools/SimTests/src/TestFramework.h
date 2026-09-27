// Minimal test framework (no exceptions) for the simulation core.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace bht
{
struct TestCase
{
	const char* Name;
	void (*Fn)();
};

inline std::vector<TestCase>& Registry()
{
	static std::vector<TestCase> Tests;
	return Tests;
}

inline int& FailureCount()
{
	static int Failures = 0;
	return Failures;
}

struct Registrar
{
	Registrar(const char* Name, void (*Fn)()) { Registry().push_back({Name, Fn}); }
};
} // namespace bht

#define BH_TEST(Name)                                            \
	static void Name();                                          \
	static bht::Registrar Name##_registrar(#Name, &Name);        \
	static void Name()

#define BH_EXPECT(Cond)                                                                   \
	do                                                                                    \
	{                                                                                     \
		if (!(Cond))                                                                      \
		{                                                                                 \
			std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #Cond);               \
			++bht::FailureCount();                                                        \
		}                                                                                 \
	} while (0)

#define BH_EXPECT_MSG(Cond, ...)                                                          \
	do                                                                                    \
	{                                                                                     \
		if (!(Cond))                                                                      \
		{                                                                                 \
			std::printf("    FAIL %s:%d: %s -- ", __FILE__, __LINE__, #Cond);             \
			std::printf(__VA_ARGS__);                                                     \
			std::printf("\n");                                                            \
			++bht::FailureCount();                                                        \
		}                                                                                 \
	} while (0)
