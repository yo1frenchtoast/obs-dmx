#include "test-harness.h"

#include <cstdio>

int main()
{
	for (const auto &c : testing::cases()) {
		const int before = testing::failures();
		testing::currentCase() = c.name;
		c.fn();
		std::printf("%s %s\n", testing::failures() == before ? "ok  " : "ECHEC", c.name.c_str());
	}

	if (testing::failures() > 0) {
		std::printf("\n%d assertion(s) en echec\n", testing::failures());
		return 1;
	}
	std::printf("\n%zu test(s) passes\n", testing::cases().size());
	return 0;
}
