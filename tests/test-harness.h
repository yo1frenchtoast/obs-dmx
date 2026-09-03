#pragma once

// Un harnais minimal : le moteur n'a aucune dependance, on ne va pas lui en
// ajouter une pour trois assertions.

#include <cstdio>
#include <string>
#include <vector>

namespace testing {

struct Case {
	std::string name;
	void (*fn)();
};

inline std::vector<Case> &cases()
{
	static std::vector<Case> all;
	return all;
}

inline int &failures()
{
	static int count = 0;
	return count;
}

inline std::string &currentCase()
{
	static std::string name;
	return name;
}

struct Registrar {
	Registrar(const char *name, void (*fn)())
	{
		cases().push_back({name, fn});
	}
};

inline void reportFailure(const char *file, int line, const std::string &message)
{
	++failures();
	std::fprintf(stderr, "  ECHEC %s:%d\n    %s\n", file, line, message.c_str());
}

} // namespace testing

#define TEST(name)                                                    \
	static void name();                                           \
	static ::testing::Registrar registrar_##name(#name, &name);   \
	static void name()

#define CHECK(cond)                                                              \
	do {                                                                     \
		if (!(cond))                                                     \
			::testing::reportFailure(__FILE__, __LINE__, #cond);     \
	} while (0)

#define CHECK_EQ(actual, expected)                                                            \
	do {                                                                                  \
		const auto actual_ = (actual);                                                \
		const auto expected_ = (expected);                                            \
		if (!(actual_ == expected_))                                                  \
			::testing::reportFailure(__FILE__, __LINE__,                          \
						 std::string(#actual) + " = " +               \
							 std::to_string(actual_) + ", attendu " + \
							 std::to_string(expected_));          \
	} while (0)
