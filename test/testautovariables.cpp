/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2010 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tokenize.h"
#include "checkautovariables.h"
#include "testsuite.h"

#include <sstream>

extern std::ostringstream errout;

class TestAutoVariables : public TestFixture
{
public:
    TestAutoVariables() : TestFixture("TestAutoVariables")
    { }

private:



    void check(const char code[])
    {
        // Tokenize..
        Tokenizer tokenizer;
        std::istringstream istr(code);
        tokenizer.tokenize(istr, "test.cpp");
        tokenizer.simplifyTokenList();

        // Assign variable ids
        tokenizer.setVarId();

        // Fill function list
        tokenizer.fillFunctionList();

        // Clear the error buffer..
        errout.str("");

        // Check auto variables
        Settings settings;
        CheckAutoVariables checkAutoVariables(&tokenizer, &settings, this);
        checkAutoVariables.autoVariables();
        checkAutoVariables.returnPointerToLocalArray();
        checkAutoVariables.returnReference();
        checkAutoVariables.returncstr();
    }

    void run()
    {
        TEST_CASE(testautovar);
        TEST_CASE(testautovar_array);
        TEST_CASE(testautovar_return);
        TEST_CASE(testautovar_extern);
        TEST_CASE(testinvaliddealloc);

        TEST_CASE(returnLocalVariable1);
        TEST_CASE(returnLocalVariable2);

        // return reference..
        TEST_CASE(returnReference);

        // return c_str()..
        TEST_CASE(returncstr);
    }



    void testautovar()
    {
        check("void func1(int **res)\n"
              "{\n"
              "    int num = 2;\n"
              "    *res = &num;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Wrong assignment of an auto-variable to an effective parameter of a function\n", errout.str());

        check("void func1(int **res)\n"
              "{\n"
              "    int num = 2;\n"
              "    foo.res = &num;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void testautovar_array()
    {
        check("void func1(int* arr[2])\n"
              "{\n"
              "    int num=2;"
              "    arr[0]=&num;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Wrong assignment of an auto-variable to an effective parameter of a function\n", errout.str());
    }

    void testautovar_return()
    {
        check("int* func1()\n"
              "{\n"
              "    int num=2;"
              "return &num;}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Return of the address of an auto-variable\n", errout.str());
    }

    void testautovar_extern()
    {
        check("struct foo *f()\n"
              "{\n"
              "    extern struct foo f;\n"
              "    return &f;\n"
              "}\n");
        ASSERT_EQUALS("", errout.str());
    }

    void testinvaliddealloc()
    {
        check("int* func1()\n"
              "{\n"
              "int a;\n"
              "char tmp[256];\n"
              "free (tmp);\n"
              "}\n");
        ASSERT_EQUALS(std::string("[test.cpp:5]: (error) Invalid deallocation\n"), errout.str());

        check("void f()\n"
              "{\n"
              "    char psz_title[10];\n"
              "    {\n"
              "        char *psz_title = 0;\n"
              "        abc(0, psz_title);\n"
              "        free(psz_title);\n"
              "    }\n"
              "}\n");
        ASSERT_EQUALS(std::string(""), errout.str());
    }

    void returnLocalVariable1()
    {
        check("char *foo()\n"
              "{\n"
              "    char str[100] = {0};\n"
              "    return str;\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:4]: (error) Returning pointer to local array variable\n", errout.str());
    }

    void returnLocalVariable2()
    {
        check("std::string foo()\n"
              "{\n"
              "    char str[100] = {0};\n"
              "    return str;\n"
              "}\n");
        ASSERT_EQUALS("", errout.str());
    }

    void returnReference()
    {
        check("std::string &foo()\n"
              "{\n"
              "    std::string s;\n"
              "    return s;\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:4]: (error) Returning reference to auto variable\n", errout.str());

        check("std::vector<int> &foo()\n"
              "{\n"
              "    std::vector<int> v;\n"
              "    return v;\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:4]: (error) Returning reference to auto variable\n", errout.str());

        check("std::vector<int> &foo()\n"
              "{\n"
              "    static std::vector<int> v;\n"
              "    return v;\n"
              "}\n");
        ASSERT_EQUALS("", errout.str());

        check("std::string hello()\n"
              "{\n"
              "     return \"hello\";\n"
              "}\n"
              "\n"
              "std::string &f()\n"
              "{\n"
              "    return hello();\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:8]: (error) Returning reference to temporary\n", errout.str());
    }

    void returncstr()
    {
        check("const char *foo()\n"
              "{\n"
              "    std::string s;\n"
              "    return s.c_str();\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:4]: (error) Returning pointer to auto variable\n", errout.str());

        check("std::string hello()\n"
              "{\n"
              "     return \"hello\";\n"
              "}\n"
              "\n"
              "const char *f()\n"
              "{\n"
              "    return hello().c_str();\n"
              "}\n");
        ASSERT_EQUALS("[test.cpp:8]: (error) Returning pointer to temporary\n", errout.str());
    }

};

REGISTER_TEST(TestAutoVariables)


