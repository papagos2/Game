// Test runner. Usage: bhtests [substring-filter]
#include "TestFramework.h"

#include <cstring>

int main(int Argc, char** Argv)
{
	const char* Filter = Argc > 1 ? Argv[1] : nullptr;
	int Ran = 0;
	for (const bht::TestCase& T : bht::Registry())
	{
		if (Filter != nullptr && std::strstr(T.Name, Filter) == nullptr)
		{
			continue;
		}
		const int Before = bht::FailureCount();
		std::printf("[ RUN ] %s\n", T.Name);
		T.Fn();
		std::printf("[ %s ] %s\n", bht::FailureCount() == Before ? " OK " : "FAIL", T.Name);
		++Ran;
	}
	std::printf("\n%d tests, %d failures\n", Ran, bht::FailureCount());
	return bht::FailureCount() == 0 ? 0 : 1;
}
