// Copyright (c) - Graphical Playground. All rights reserved.

#include "Utilities/StringUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace GP;
using namespace GP::Utilities;
using namespace GP::Container;

TEST_CASE("IsAlpha", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All lowercase ASCII letters return true")
    {
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE(IsAlpha(c)); }
    }

    SECTION("All uppercase ASCII letters return true")
    {
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE(IsAlpha(c)); }
    }

    SECTION("Decimal digits return false")
    {
        for (char c = '0'; c <= '9'; ++c) { REQUIRE_FALSE(IsAlpha(c)); }
    }

    SECTION("Boundary characters adjacent to alpha ranges return false")
    {
        REQUIRE_FALSE(IsAlpha('@'));   // One before 'A'
        REQUIRE_FALSE(IsAlpha('['));   // One after 'Z'
        REQUIRE_FALSE(IsAlpha('`'));   // One before 'a'
        REQUIRE_FALSE(IsAlpha('{'));   // One after 'z'
    }

    SECTION("Common non-alpha characters return false")
    {
        REQUIRE_FALSE(IsAlpha(' '));
        REQUIRE_FALSE(IsAlpha('!'));
        REQUIRE_FALSE(IsAlpha('_'));
        REQUIRE_FALSE(IsAlpha('-'));
        REQUIRE_FALSE(IsAlpha('\0'));
        REQUIRE_FALSE(IsAlpha('\n'));
    }
}

TEST_CASE("IsDigit", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Digits 0-9 return true")
    {
        for (char c = '0'; c <= '9'; ++c) { REQUIRE(IsDigit(c)); }
    }

    SECTION("Boundary characters adjacent to digit range return false")
    {
        REQUIRE_FALSE(IsDigit('/'));   // One before '0'
        REQUIRE_FALSE(IsDigit(':'));   // One after '9'
    }

    SECTION("Letters return false")
    {
        REQUIRE_FALSE(IsDigit('a'));
        REQUIRE_FALSE(IsDigit('Z'));
    }

    SECTION("Common non-digit characters return false")
    {
        REQUIRE_FALSE(IsDigit(' '));
        REQUIRE_FALSE(IsDigit('\0'));
        REQUIRE_FALSE(IsDigit('!'));
    }
}

TEST_CASE("IsAlNum", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All lowercase letters return true")
    {
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE(IsAlNum(c)); }
    }

    SECTION("All uppercase letters return true")
    {
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE(IsAlNum(c)); }
    }

    SECTION("All digits return true")
    {
        for (char c = '0'; c <= '9'; ++c) { REQUIRE(IsAlNum(c)); }
    }

    SECTION("Non-alphanumeric characters return false")
    {
        REQUIRE_FALSE(IsAlNum(' '));
        REQUIRE_FALSE(IsAlNum('_'));
        REQUIRE_FALSE(IsAlNum('!'));
        REQUIRE_FALSE(IsAlNum('-'));
        REQUIRE_FALSE(IsAlNum('\0'));
    }
}

TEST_CASE("IsSpace", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All standard ASCII whitespace characters return true")
    {
        REQUIRE(IsSpace(' '));    // 0x20 space
        REQUIRE(IsSpace('\t'));   // 0x09 horizontal tab
        REQUIRE(IsSpace('\n'));   // 0x0A line feed
        REQUIRE(IsSpace('\v'));   // 0x0B vertical tab
        REQUIRE(IsSpace('\f'));   // 0x0C form feed
        REQUIRE(IsSpace('\r'));   // 0x0D carriage return
    }

    SECTION("Letters and digits return false")
    {
        REQUIRE_FALSE(IsSpace('a'));
        REQUIRE_FALSE(IsSpace('Z'));
        REQUIRE_FALSE(IsSpace('0'));
    }

    SECTION("Null terminator and other controls return false")
    {
        REQUIRE_FALSE(IsSpace('\0'));
        REQUIRE_FALSE(IsSpace('\x08'));   // backspace
    }

    SECTION("Printable non-space characters return false")
    {
        REQUIRE_FALSE(IsSpace('!'));
        REQUIRE_FALSE(IsSpace('_'));
    }
}

TEST_CASE("IsUpper", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All uppercase letters return true")
    {
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE(IsUpper(c)); }
    }

    SECTION("All lowercase letters return false")
    {
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE_FALSE(IsUpper(c)); }
    }

    SECTION("Boundary characters adjacent to uppercase range return false")
    {
        REQUIRE_FALSE(IsUpper('@'));   // One before 'A'
        REQUIRE_FALSE(IsUpper('['));   // One after 'Z'
    }

    SECTION("Digits and special characters return false")
    {
        REQUIRE_FALSE(IsUpper('0'));
        REQUIRE_FALSE(IsUpper(' '));
        REQUIRE_FALSE(IsUpper('!'));
    }
}

TEST_CASE("IsLower", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All lowercase letters return true")
    {
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE(IsLower(c)); }
    }

    SECTION("All uppercase letters return false")
    {
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE_FALSE(IsLower(c)); }
    }

    SECTION("Boundary characters adjacent to lowercase range return false")
    {
        REQUIRE_FALSE(IsLower('`'));   // One before 'a'
        REQUIRE_FALSE(IsLower('{'));   // One after 'z'
    }

    SECTION("Digits and special characters return false")
    {
        REQUIRE_FALSE(IsLower('0'));
        REQUIRE_FALSE(IsLower(' '));
        REQUIRE_FALSE(IsLower('!'));
    }
}

TEST_CASE("IsHexDigit", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Decimal digits 0-9 return true")
    {
        for (char c = '0'; c <= '9'; ++c) { REQUIRE(IsHexDigit(c)); }
    }

    SECTION("Lowercase hex letters a-f return true")
    {
        for (char c = 'a'; c <= 'f'; ++c) { REQUIRE(IsHexDigit(c)); }
    }

    SECTION("Uppercase hex letters A-F return true")
    {
        for (char c = 'A'; c <= 'F'; ++c) { REQUIRE(IsHexDigit(c)); }
    }

    SECTION("Letters g-z return false")
    {
        for (char c = 'g'; c <= 'z'; ++c) { REQUIRE_FALSE(IsHexDigit(c)); }
    }

    SECTION("Letters G-Z return false")
    {
        for (char c = 'G'; c <= 'Z'; ++c) { REQUIRE_FALSE(IsHexDigit(c)); }
    }

    SECTION("Special characters return false")
    {
        REQUIRE_FALSE(IsHexDigit(' '));
        REQUIRE_FALSE(IsHexDigit('!'));
        REQUIRE_FALSE(IsHexDigit('\0'));
    }
}

TEST_CASE("IsPrint", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All printable ASCII characters (0x20-0x7E) return true")
    {
        for (int c = 0x20; c <= 0x7E; ++c) { REQUIRE(IsPrint(static_cast<char>(c))); }
    }

    SECTION("Control characters below 0x20 return false")
    {
        REQUIRE_FALSE(IsPrint('\0'));
        REQUIRE_FALSE(IsPrint('\t'));
        REQUIRE_FALSE(IsPrint('\n'));
        REQUIRE_FALSE(IsPrint('\r'));
        REQUIRE_FALSE(IsPrint('\x1F'));
    }

    SECTION("DEL (0x7F) returns false") { REQUIRE_FALSE(IsPrint('\x7F')); }
}

TEST_CASE("ToUpper (char)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Lowercase letters are converted to uppercase")
    {
        REQUIRE(ToUpper('a') == 'A');
        REQUIRE(ToUpper('z') == 'Z');
        REQUIRE(ToUpper('m') == 'M');
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE(ToUpper(c) == static_cast<char>(c - 32)); }
    }

    SECTION("Uppercase letters are returned unchanged")
    {
        REQUIRE(ToUpper('A') == 'A');
        REQUIRE(ToUpper('Z') == 'Z');
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE(ToUpper(c) == c); }
    }

    SECTION("Non-alpha characters are returned unchanged")
    {
        REQUIRE(ToUpper('0') == '0');
        REQUIRE(ToUpper('9') == '9');
        REQUIRE(ToUpper(' ') == ' ');
        REQUIRE(ToUpper('!') == '!');
        REQUIRE(ToUpper('_') == '_');
    }
}

TEST_CASE("ToLower (char)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Uppercase letters are converted to lowercase")
    {
        REQUIRE(ToLower('A') == 'a');
        REQUIRE(ToLower('Z') == 'z');
        REQUIRE(ToLower('M') == 'm');
        for (char c = 'A'; c <= 'Z'; ++c) { REQUIRE(ToLower(c) == static_cast<char>(c + 32)); }
    }

    SECTION("Lowercase letters are returned unchanged")
    {
        REQUIRE(ToLower('a') == 'a');
        REQUIRE(ToLower('z') == 'z');
        for (char c = 'a'; c <= 'z'; ++c) { REQUIRE(ToLower(c) == c); }
    }

    SECTION("Non-alpha characters are returned unchanged")
    {
        REQUIRE(ToLower('0') == '0');
        REQUIRE(ToLower('9') == '9');
        REQUIRE(ToLower(' ') == ' ');
        REQUIRE(ToLower('!') == '!');
    }
}

TEST_CASE("ToUpperStr", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All lowercase characters are converted to uppercase")
    {
        auto result = ToUpperStr(TBasicStringView<char>("hello world"));
        REQUIRE(result == "HELLO WORLD");
    }

    SECTION("Mixed case is fully uppercased")
    {
        auto result = ToUpperStr(TBasicStringView<char>("Hello World"));
        REQUIRE(result == "HELLO WORLD");
    }

    SECTION("Already all-uppercase string is returned unchanged")
    {
        auto result = ToUpperStr(TBasicStringView<char>("HELLO"));
        REQUIRE(result == "HELLO");
    }

    SECTION("Non-alpha characters are preserved")
    {
        auto result = ToUpperStr(TBasicStringView<char>("abc 123!"));
        REQUIRE(result == "ABC 123!");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToUpperStr(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToLowerStr", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All uppercase characters are converted to lowercase")
    {
        auto result = ToLowerStr(TBasicStringView<char>("HELLO WORLD"));
        REQUIRE(result == "hello world");
    }

    SECTION("Mixed case is fully lowercased")
    {
        auto result = ToLowerStr(TBasicStringView<char>("Hello World"));
        REQUIRE(result == "hello world");
    }

    SECTION("Already all-lowercase string is returned unchanged")
    {
        auto result = ToLowerStr(TBasicStringView<char>("hello"));
        REQUIRE(result == "hello");
    }

    SECTION("Non-alpha characters are preserved")
    {
        auto result = ToLowerStr(TBasicStringView<char>("ABC 123!"));
        REQUIRE(result == "abc 123!");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToLowerStr(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToTitleCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Each space-separated word has its first letter uppercased")
    {
        auto result = ToTitleCase(TBasicStringView<char>("hello world"));
        REQUIRE(result == "Hello World");
    }

    SECTION("All-uppercase input is converted correctly")
    {
        auto result = ToTitleCase(TBasicStringView<char>("HELLO WORLD"));
        REQUIRE(result == "Hello World");
    }

    SECTION("Single word is capitalized")
    {
        auto result = ToTitleCase(TBasicStringView<char>("hello"));
        REQUIRE(result == "Hello");
    }

    SECTION("Already title-cased string is returned unchanged")
    {
        auto result = ToTitleCase(TBasicStringView<char>("Hello World"));
        REQUIRE(result == "Hello World");
    }

    SECTION("Multiple consecutive spaces are preserved, both adjacent words are capitalized")
    {
        auto result = ToTitleCase(TBasicStringView<char>("hello  world"));
        REQUIRE(result == "Hello  World");
    }

    SECTION("Leading whitespace: first non-space character is uppercased")
    {
        auto result = ToTitleCase(TBasicStringView<char>(" hello"));
        REQUIRE(result == " Hello");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToTitleCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToCamelCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("snake_case is converted to lowerCamelCase")
    {
        auto result = ToCamelCase(TBasicStringView<char>("my_variable_name"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("PascalCase is converted to lowerCamelCase")
    {
        auto result = ToCamelCase(TBasicStringView<char>("MyVariableName"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("kebab-case is converted to lowerCamelCase")
    {
        auto result = ToCamelCase(TBasicStringView<char>("my-variable-name"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("Dot-separated is converted to lowerCamelCase")
    {
        auto result = ToCamelCase(TBasicStringView<char>("my.variable.name"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("Already lowerCamelCase is normalized correctly")
    {
        auto result = ToCamelCase(TBasicStringView<char>("myVariableName"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("Single word is fully lowercased")
    {
        auto result = ToCamelCase(TBasicStringView<char>("Hello"));
        REQUIRE(result == "hello");
    }

    SECTION("Acronym word boundary: HTMLParser is split at acronym end")
    {
        auto result = ToCamelCase(TBasicStringView<char>("HTMLParser"));
        REQUIRE(result == "htmlParser");
    }

    SECTION("Acronym mid-string: getHTTPSUrl splits correctly")
    {
        auto result = ToCamelCase(TBasicStringView<char>("getHTTPSUrl"));
        REQUIRE(result == "getHttpsUrl");
    }

    SECTION("SCREAMING_SNAKE_CASE is converted to lowerCamelCase")
    {
        auto result = ToCamelCase(TBasicStringView<char>("MY_VARIABLE_NAME"));
        REQUIRE(result == "myVariableName");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToCamelCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToPascalCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("snake_case is converted to PascalCase")
    {
        auto result = ToPascalCase(TBasicStringView<char>("my_variable_name"));
        REQUIRE(result == "MyVariableName");
    }

    SECTION("camelCase is converted to PascalCase")
    {
        auto result = ToPascalCase(TBasicStringView<char>("myVariableName"));
        REQUIRE(result == "MyVariableName");
    }

    SECTION("kebab-case is converted to PascalCase")
    {
        auto result = ToPascalCase(TBasicStringView<char>("my-variable-name"));
        REQUIRE(result == "MyVariableName");
    }

    SECTION("Already PascalCase is normalized correctly")
    {
        auto result = ToPascalCase(TBasicStringView<char>("MyVariableName"));
        REQUIRE(result == "MyVariableName");
    }

    SECTION("Single word is capitalized")
    {
        auto result = ToPascalCase(TBasicStringView<char>("hello"));
        REQUIRE(result == "Hello");
    }

    SECTION("Acronym word boundary: HTMLParser")
    {
        auto result = ToPascalCase(TBasicStringView<char>("HTMLParser"));
        REQUIRE(result == "HtmlParser");
    }

    SECTION("SCREAMING_SNAKE_CASE is converted to PascalCase")
    {
        auto result = ToPascalCase(TBasicStringView<char>("MY_VARIABLE_NAME"));
        REQUIRE(result == "MyVariableName");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToPascalCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToSnakeCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("camelCase is converted to snake_case")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("myVariableName"));
        REQUIRE(result == "my_variable_name");
    }

    SECTION("PascalCase is converted to snake_case")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("MyVariableName"));
        REQUIRE(result == "my_variable_name");
    }

    SECTION("Already snake_case is returned unchanged")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("my_variable_name"));
        REQUIRE(result == "my_variable_name");
    }

    SECTION("kebab-case is converted to snake_case")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("my-variable-name"));
        REQUIRE(result == "my_variable_name");
    }

    SECTION("Acronym: HTMLParser")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("HTMLParser"));
        REQUIRE(result == "html_parser");
    }

    SECTION("Single word is fully lowercased")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("Hello"));
        REQUIRE(result == "hello");
    }

    SECTION("SCREAMING_SNAKE_CASE is normalized to snake_case")
    {
        auto result = ToSnakeCase(TBasicStringView<char>("MY_VARIABLE_NAME"));
        REQUIRE(result == "my_variable_name");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToSnakeCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToKebabCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("camelCase is converted to kebab-case")
    {
        auto result = ToKebabCase(TBasicStringView<char>("myVariableName"));
        REQUIRE(result == "my-variable-name");
    }

    SECTION("PascalCase is converted to kebab-case")
    {
        auto result = ToKebabCase(TBasicStringView<char>("MyVariableName"));
        REQUIRE(result == "my-variable-name");
    }

    SECTION("snake_case is converted to kebab-case")
    {
        auto result = ToKebabCase(TBasicStringView<char>("my_variable_name"));
        REQUIRE(result == "my-variable-name");
    }

    SECTION("Already kebab-case is returned unchanged")
    {
        auto result = ToKebabCase(TBasicStringView<char>("my-variable-name"));
        REQUIRE(result == "my-variable-name");
    }

    SECTION("Acronym: HTMLParser")
    {
        auto result = ToKebabCase(TBasicStringView<char>("HTMLParser"));
        REQUIRE(result == "html-parser");
    }

    SECTION("Single word is fully lowercased")
    {
        auto result = ToKebabCase(TBasicStringView<char>("Hello"));
        REQUIRE(result == "hello");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToKebabCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("ToScreamingSnakeCase", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("camelCase is converted to SCREAMING_SNAKE_CASE")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("myVariableName"));
        REQUIRE(result == "MY_VARIABLE_NAME");
    }

    SECTION("PascalCase is converted to SCREAMING_SNAKE_CASE")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("MyVariableName"));
        REQUIRE(result == "MY_VARIABLE_NAME");
    }

    SECTION("snake_case is converted to SCREAMING_SNAKE_CASE")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("my_variable_name"));
        REQUIRE(result == "MY_VARIABLE_NAME");
    }

    SECTION("kebab-case is converted to SCREAMING_SNAKE_CASE")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("my-variable-name"));
        REQUIRE(result == "MY_VARIABLE_NAME");
    }

    SECTION("Already SCREAMING_SNAKE_CASE is returned unchanged")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("MY_VARIABLE_NAME"));
        REQUIRE(result == "MY_VARIABLE_NAME");
    }

    SECTION("Single word is fully uppercased")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("hello"));
        REQUIRE(result == "HELLO");
    }

    SECTION("Acronym: HTMLParser")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>("HTMLParser"));
        REQUIRE(result == "HTML_PARSER");
    }

    SECTION("Empty string returns empty")
    {
        auto result = ToScreamingSnakeCase(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("TrimLeftView", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Leading spaces are removed")
    {
        auto result = TrimLeftView(TBasicStringView<char>("   hello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Leading tab and newline are removed")
    {
        auto result = TrimLeftView(TBasicStringView<char>("\t\nhello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Trailing whitespace is not affected")
    {
        auto result = TrimLeftView(TBasicStringView<char>("hello   "));
        REQUIRE(result == TBasicStringView<char>("hello   "));
    }

    SECTION("No leading whitespace returns original view unchanged")
    {
        auto result = TrimLeftView(TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("All-whitespace string returns empty view")
    {
        auto result = TrimLeftView(TBasicStringView<char>("   "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty view")
    {
        auto result = TrimLeftView(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Result is a zero-copy subview pointing into the original data")
    {
        TBasicStringView<char> sv("  hello");
        auto result = TrimLeftView(sv);
        REQUIRE(result.Data() == sv.Data() + 2);
        REQUIRE(result.Size() == 5);
    }
}

TEST_CASE("TrimRightView", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Trailing spaces are removed")
    {
        auto result = TrimRightView(TBasicStringView<char>("hello   "));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Trailing mixed whitespace is removed")
    {
        auto result = TrimRightView(TBasicStringView<char>("hello\t\r\n"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Leading whitespace is not affected")
    {
        auto result = TrimRightView(TBasicStringView<char>("   hello"));
        REQUIRE(result == TBasicStringView<char>("   hello"));
    }

    SECTION("No trailing whitespace returns original view unchanged")
    {
        auto result = TrimRightView(TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("All-whitespace string returns empty view")
    {
        auto result = TrimRightView(TBasicStringView<char>("   "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty view")
    {
        auto result = TrimRightView(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Result is a zero-copy subview pointing into the original data")
    {
        TBasicStringView<char> sv("hello  ");
        auto result = TrimRightView(sv);
        REQUIRE(result.Data() == sv.Data());
        REQUIRE(result.Size() == 5);
    }
}

TEST_CASE("TrimView", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Both leading and trailing whitespace are removed")
    {
        auto result = TrimView(TBasicStringView<char>("  hello  "));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Only leading whitespace is removed when trailing is absent")
    {
        auto result = TrimView(TBasicStringView<char>("  hello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Only trailing whitespace is removed when leading is absent")
    {
        auto result = TrimView(TBasicStringView<char>("hello  "));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("String without whitespace is returned unchanged")
    {
        auto result = TrimView(TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("All-whitespace string returns empty view")
    {
        auto result = TrimView(TBasicStringView<char>("\t  \r\n"));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty view")
    {
        auto result = TrimView(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Mixed whitespace types are all trimmed")
    {
        auto result = TrimView(TBasicStringView<char>("\t \n hello \r\n"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }
}

TEST_CASE("TrimLeft (Owning)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Leading whitespace is removed and a new string is returned")
    {
        auto result = TrimLeft(TBasicStringView<char>("  hello"));
        REQUIRE(result == "hello");
    }

    SECTION("No leading whitespace produces an equal copy")
    {
        auto result = TrimLeft(TBasicStringView<char>("hello"));
        REQUIRE(result == "hello");
    }

    SECTION("All-whitespace string returns empty")
    {
        auto result = TrimLeft(TBasicStringView<char>("   "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty")
    {
        auto result = TrimLeft(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("TrimRight (Owning)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Trailing whitespace is removed and a new string is returned")
    {
        auto result = TrimRight(TBasicStringView<char>("hello  "));
        REQUIRE(result == "hello");
    }

    SECTION("No trailing whitespace produces an equal copy")
    {
        auto result = TrimRight(TBasicStringView<char>("hello"));
        REQUIRE(result == "hello");
    }

    SECTION("All-whitespace string returns empty")
    {
        auto result = TrimRight(TBasicStringView<char>("   "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty")
    {
        auto result = TrimRight(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }
}

TEST_CASE("Trim (Owning)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Both leading and trailing whitespace are removed")
    {
        auto result = Trim(TBasicStringView<char>("  hello  "));
        REQUIRE(result == "hello");
    }

    SECTION("String without whitespace produces an equal copy")
    {
        auto result = Trim(TBasicStringView<char>("hello"));
        REQUIRE(result == "hello");
    }

    SECTION("All-whitespace string returns empty")
    {
        auto result = Trim(TBasicStringView<char>("   "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string returns empty")
    {
        auto result = Trim(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Mixed whitespace types are all trimmed")
    {
        auto result = Trim(TBasicStringView<char>("\t \n hello \r\n"));
        REQUIRE(result == "hello");
    }
}

TEST_CASE("PadLeft", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Pads on the left with the specified fill character")
    {
        auto result = PadLeft(TBasicStringView<char>("hi"), SizeT(5), ' ');
        REQUIRE(result == "   hi");
    }

    SECTION("Pads with zero character for numeric-style formatting")
    {
        auto result = PadLeft(TBasicStringView<char>("42"), SizeT(6), '0');
        REQUIRE(result == "000042");
    }

    SECTION("Pads with a custom fill character")
    {
        auto result = PadLeft(TBasicStringView<char>("hi"), SizeT(5), '-');
        REQUIRE(result == "---hi");
    }

    SECTION("String already at totalWidth is returned unchanged")
    {
        auto result = PadLeft(TBasicStringView<char>("hello"), SizeT(5), ' ');
        REQUIRE(result == "hello");
    }

    SECTION("String exceeding totalWidth is returned unchanged")
    {
        auto result = PadLeft(TBasicStringView<char>("toolong"), SizeT(3), ' ');
        REQUIRE(result == "toolong");
    }

    SECTION("Empty string is padded to full width")
    {
        auto result = PadLeft(TBasicStringView<char>(""), SizeT(3), '*');
        REQUIRE(result == "***");
    }

    SECTION("Width of zero returns the string unchanged")
    {
        auto result = PadLeft(TBasicStringView<char>("hi"), SizeT(0), ' ');
        REQUIRE(result == "hi");
    }
}

TEST_CASE("PadRight", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Pads on the right with spaces")
    {
        auto result = PadRight(TBasicStringView<char>("hi"), SizeT(5), ' ');
        REQUIRE(result == "hi   ");
    }

    SECTION("Pads with a custom fill character")
    {
        auto result = PadRight(TBasicStringView<char>("hi"), SizeT(5), '-');
        REQUIRE(result == "hi---");
    }

    SECTION("String already at totalWidth is returned unchanged")
    {
        auto result = PadRight(TBasicStringView<char>("hello"), SizeT(5), ' ');
        REQUIRE(result == "hello");
    }

    SECTION("String exceeding totalWidth is returned unchanged")
    {
        auto result = PadRight(TBasicStringView<char>("toolong"), SizeT(3), ' ');
        REQUIRE(result == "toolong");
    }

    SECTION("Empty string is padded to full width")
    {
        auto result = PadRight(TBasicStringView<char>(""), SizeT(3), '*');
        REQUIRE(result == "***");
    }

    SECTION("Width of zero returns the string unchanged")
    {
        auto result = PadRight(TBasicStringView<char>("hi"), SizeT(0), ' ');
        REQUIRE(result == "hi");
    }
}

TEST_CASE("PadCenter", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Even padding is split equally between left and right")
    {
        auto result = PadCenter(TBasicStringView<char>("hi"), SizeT(6), ' ');
        REQUIRE(result == "  hi  ");
    }

    SECTION("Odd padding places the extra fill character on the right side")
    {
        auto result = PadCenter(TBasicStringView<char>("hi"), SizeT(5), ' ');
        REQUIRE(result == " hi  ");
    }

    SECTION("Pads with a custom fill character")
    {
        auto result = PadCenter(TBasicStringView<char>("x"), SizeT(5), '-');
        REQUIRE(result == "--x--");
    }

    SECTION("String already at totalWidth is returned unchanged")
    {
        auto result = PadCenter(TBasicStringView<char>("hello"), SizeT(5), ' ');
        REQUIRE(result == "hello");
    }

    SECTION("String exceeding totalWidth is returned unchanged")
    {
        auto result = PadCenter(TBasicStringView<char>("toolong"), SizeT(3), ' ');
        REQUIRE(result == "toolong");
    }

    SECTION("Empty string is padded to full width")
    {
        auto result = PadCenter(TBasicStringView<char>(""), SizeT(4), '*');
        REQUIRE(result == "****");
    }

    SECTION("Width of zero returns the string unchanged")
    {
        auto result = PadCenter(TBasicStringView<char>("hi"), SizeT(0), ' ');
        REQUIRE(result == "hi");
    }
}

TEST_CASE("Repeat", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Repeats the string the given number of times")
    {
        auto result = Repeat(TBasicStringView<char>("ab"), SizeT(3));
        REQUIRE(result == "ababab");
    }

    SECTION("Count of 1 returns an equal copy of the string")
    {
        auto result = Repeat(TBasicStringView<char>("hello"), SizeT(1));
        REQUIRE(result == "hello");
    }

    SECTION("Count of 0 returns an empty string")
    {
        auto result = Repeat(TBasicStringView<char>("hello"), SizeT(0));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty string with any positive count returns empty")
    {
        auto result = Repeat(TBasicStringView<char>(""), SizeT(5));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Single character repeated")
    {
        auto result = Repeat(TBasicStringView<char>("x"), SizeT(4));
        REQUIRE(result == "xxxx");
    }
}

TEST_CASE("ReplaceAll", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Replaces all non-overlapping occurrences left-to-right")
    {
        auto result = ReplaceAll(
            TBasicStringView<char>("hello world hello"), TBasicStringView<char>("hello"), TBasicStringView<char>("hi")
        );
        REQUIRE(result == "hi world hi");
    }

    SECTION("No occurrences found returns an equal copy of the source")
    {
        auto result = ReplaceAll(
            TBasicStringView<char>("hello world"), TBasicStringView<char>("foo"), TBasicStringView<char>("bar")
        );
        REQUIRE(result == "hello world");
    }

    SECTION("Empty 'from' string returns the source unchanged")
    {
        auto result =
            ReplaceAll(TBasicStringView<char>("hello"), TBasicStringView<char>(""), TBasicStringView<char>("x"));
        REQUIRE(result == "hello");
    }

    SECTION("Replacing with empty string deletes the matched text")
    {
        auto result =
            ReplaceAll(TBasicStringView<char>("hello world"), TBasicStringView<char>("l"), TBasicStringView<char>(""));
        REQUIRE(result == "heo word");
    }

    SECTION("Replacement string is longer than the matched text")
    {
        auto result =
            ReplaceAll(TBasicStringView<char>("aaa"), TBasicStringView<char>("a"), TBasicStringView<char>("bb"));
        REQUIRE(result == "bbbbbb");
    }

    SECTION("Replacement string is shorter than the matched text")
    {
        auto result = ReplaceAll(
            TBasicStringView<char>("hello world"), TBasicStringView<char>("world"), TBasicStringView<char>("x")
        );
        REQUIRE(result == "hello x");
    }

    SECTION("Entire string is replaced when 'from' equals the whole string")
    {
        auto result = ReplaceAll(
            TBasicStringView<char>("hello"), TBasicStringView<char>("hello"), TBasicStringView<char>("world")
        );
        REQUIRE(result == "world");
    }

    SECTION("Newly inserted text is not re-scanned (avoids infinite expansion)")
    {
        // Replacing every "a" with "aa" should yield exactly four 'a's, not loop forever.
        auto result =
            ReplaceAll(TBasicStringView<char>("aa"), TBasicStringView<char>("a"), TBasicStringView<char>("aa"));
        REQUIRE(result == "aaaa");
    }

    SECTION("Empty source string returns empty")
    {
        auto result = ReplaceAll(TBasicStringView<char>(""), TBasicStringView<char>("a"), TBasicStringView<char>("b"));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Replaces adjacent matches correctly")
    {
        auto result =
            ReplaceAll(TBasicStringView<char>("aabbcc"), TBasicStringView<char>("bb"), TBasicStringView<char>("XX"));
        REQUIRE(result == "aaXXcc");
    }
}

TEST_CASE("CountOccurrences (substring)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Multiple non-overlapping occurrences are counted")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("ababab"), TBasicStringView<char>("ab")) == 3);
    }

    SECTION("Returns 0 when the substring is not present")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello"), TBasicStringView<char>("xyz")) == 0);
    }

    SECTION("Empty substring returns 0")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello"), TBasicStringView<char>("")) == 0);
    }

    SECTION("Empty source string returns 0")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>(""), TBasicStringView<char>("a")) == 0);
    }

    SECTION("Single occurrence is counted")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello world"), TBasicStringView<char>("world")) == 1);
    }

    SECTION("Non-overlapping semantics: 'aa' in 'aaaa' counts 2, not 3")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("aaaa"), TBasicStringView<char>("aa")) == 2);
    }

    SECTION("Substring same length as source counts 1 when equal")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello"), TBasicStringView<char>("hello")) == 1);
    }
}

TEST_CASE("CountOccurrences (char)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Counts all occurrences of the given character")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello world"), 'l') == 3);
    }

    SECTION("Returns 0 when character is not present")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("hello"), 'z') == 0);
    }

    SECTION("Empty source string returns 0") { REQUIRE(CountOccurrences(TBasicStringView<char>(""), 'a') == 0); }

    SECTION("Counts single occurrence") { REQUIRE(CountOccurrences(TBasicStringView<char>("hello"), 'h') == 1); }

    SECTION("Counts when every character matches")
    {
        REQUIRE(CountOccurrences(TBasicStringView<char>("aaaa"), 'a') == 4);
    }
}

TEST_CASE("StripPrefix", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Prefix is removed when present at the start")
    {
        auto result = StripPrefix(TBasicStringView<char>("hello world"), TBasicStringView<char>("hello "));
        REQUIRE(result == TBasicStringView<char>("world"));
    }

    SECTION("Returns the original view when the prefix is absent")
    {
        auto result = StripPrefix(TBasicStringView<char>("hello world"), TBasicStringView<char>("world"));
        REQUIRE(result == TBasicStringView<char>("hello world"));
    }

    SECTION("Empty prefix always matches and returns the original view unchanged")
    {
        auto result = StripPrefix(TBasicStringView<char>("hello"), TBasicStringView<char>(""));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Stripping the entire string returns an empty view")
    {
        auto result = StripPrefix(TBasicStringView<char>("hello"), TBasicStringView<char>("hello"));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Result is a zero-copy subview pointing into the original data")
    {
        TBasicStringView<char> sv("prefixContent");
        auto result = StripPrefix(sv, TBasicStringView<char>("prefix"));
        REQUIRE(result.Data() == sv.Data() + 6);
        REQUIRE(result.Size() == 7);
    }

    SECTION("Prefix longer than string returns original view")
    {
        auto result = StripPrefix(TBasicStringView<char>("hi"), TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hi"));
    }
}

TEST_CASE("StripSuffix", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Suffix is removed when present at the end")
    {
        auto result = StripSuffix(TBasicStringView<char>("hello world"), TBasicStringView<char>(" world"));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Returns the original view when the suffix is absent")
    {
        auto result = StripSuffix(TBasicStringView<char>("hello world"), TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hello world"));
    }

    SECTION("Empty suffix always matches and returns the original view unchanged")
    {
        auto result = StripSuffix(TBasicStringView<char>("hello"), TBasicStringView<char>(""));
        REQUIRE(result == TBasicStringView<char>("hello"));
    }

    SECTION("Stripping the entire string returns an empty view")
    {
        auto result = StripSuffix(TBasicStringView<char>("hello"), TBasicStringView<char>("hello"));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Result is a zero-copy subview pointing into the original data")
    {
        TBasicStringView<char> sv("ContentSuffix");
        auto result = StripSuffix(sv, TBasicStringView<char>("Suffix"));
        REQUIRE(result.Data() == sv.Data());
        REQUIRE(result.Size() == 7);
    }

    SECTION("Suffix longer than string returns original view")
    {
        auto result = StripSuffix(TBasicStringView<char>("hi"), TBasicStringView<char>("hello"));
        REQUIRE(result == TBasicStringView<char>("hi"));
    }
}

TEST_CASE("Split (char delimiter)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Splits a string into parts separated by the delimiter")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a,b,c"), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1] == TBasicStringView<char>("b"));
        REQUIRE(parts[2] == TBasicStringView<char>("c"));
    }

    SECTION("No delimiter found produces one part equal to the full string")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("hello"), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0] == TBasicStringView<char>("hello"));
    }

    SECTION("Consecutive delimiters produce empty parts between them")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a,,b"), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1].IsEmpty());
        REQUIRE(parts[2] == TBasicStringView<char>("b"));
    }

    SECTION("Delimiter at the start produces an empty first part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>(",a"), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0].IsEmpty());
        REQUIRE(parts[1] == TBasicStringView<char>("a"));
    }

    SECTION("Delimiter at the end produces an empty last part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a,"), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1].IsEmpty());
    }

    SECTION("MaxParts limits the number of emitted parts; remainder in last part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a,b,c,d"), ',', std::back_inserter(parts), SizeT(2));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1] == TBasicStringView<char>("b,c,d"));
    }

    SECTION("MaxParts of 1 returns the whole string as one part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a,b,c"), ',', std::back_inserter(parts), SizeT(1));
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0] == TBasicStringView<char>("a,b,c"));
    }

    SECTION("Empty source string produces one empty part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>(""), ',', std::back_inserter(parts));
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0].IsEmpty());
    }
}

TEST_CASE("Split (string delimiter)", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Splits a string by a multi-character delimiter")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a::b::c"), TBasicStringView<char>("::"), std::back_inserter(parts));
        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1] == TBasicStringView<char>("b"));
        REQUIRE(parts[2] == TBasicStringView<char>("c"));
    }

    SECTION("Empty delimiter returns the entire string as one part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("hello"), TBasicStringView<char>(""), std::back_inserter(parts));
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0] == TBasicStringView<char>("hello"));
    }

    SECTION("MaxParts limits the number of emitted parts; remainder in last part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("a::b::c::d"), TBasicStringView<char>("::"), std::back_inserter(parts), SizeT(2));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0] == TBasicStringView<char>("a"));
        REQUIRE(parts[1] == TBasicStringView<char>("b::c::d"));
    }

    SECTION("No occurrences of delimiter produce one part equal to the full string")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("hello"), TBasicStringView<char>("::"), std::back_inserter(parts));
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0] == TBasicStringView<char>("hello"));
    }

    SECTION("Delimiter at start produces an empty first part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("::hello"), TBasicStringView<char>("::"), std::back_inserter(parts));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0].IsEmpty());
        REQUIRE(parts[1] == TBasicStringView<char>("hello"));
    }

    SECTION("Delimiter at end produces an empty last part")
    {
        std::vector<TBasicStringView<char>> parts;
        Split(TBasicStringView<char>("hello::"), TBasicStringView<char>("::"), std::back_inserter(parts));
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0] == TBasicStringView<char>("hello"));
        REQUIRE(parts[1].IsEmpty());
    }
}

TEST_CASE("Join", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Joins multiple strings with the given separator")
    {
        std::vector<TBasicString<char>> words = {
            TBasicString<char>("a"),
            TBasicString<char>("b"),
            TBasicString<char>("c"),
        };
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>(", "));
        REQUIRE(result == "a, b, c");
    }

    SECTION("Single element is returned without any separator")
    {
        std::vector<TBasicString<char>> words = { TBasicString<char>("hello") };
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>(", "));
        REQUIRE(result == "hello");
    }

    SECTION("Empty range returns an empty string")
    {
        std::vector<TBasicString<char>> words;
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>(", "));
        REQUIRE(result.IsEmpty());
    }

    SECTION("Empty separator concatenates directly")
    {
        std::vector<TBasicString<char>> words = {
            TBasicString<char>("a"),
            TBasicString<char>("b"),
            TBasicString<char>("c"),
        };
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>(""));
        REQUIRE(result == "abc");
    }

    SECTION("Joins TBasicStringView elements")
    {
        std::vector<TBasicStringView<char>> words = {
            TBasicStringView<char>("hello"),
            TBasicStringView<char>("world"),
        };
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>("-"));
        REQUIRE(result == "hello-world");
    }

    SECTION("Multi-character separator is inserted between each pair of elements")
    {
        std::vector<TBasicString<char>> words = {
            TBasicString<char>("one"),
            TBasicString<char>("two"),
            TBasicString<char>("three"),
        };
        auto result = Join(words.begin(), words.end(), TBasicStringView<char>(" | "));
        REQUIRE(result == "one | two | three");
    }
}

TEST_CASE("Reverse", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("Reverses a normal string")
    {
        auto result = Reverse(TBasicStringView<char>("hello"));
        REQUIRE(result == "olleh");
    }

    SECTION("Palindrome is returned unchanged")
    {
        auto result = Reverse(TBasicStringView<char>("racecar"));
        REQUIRE(result == "racecar");
    }

    SECTION("Single character is returned unchanged")
    {
        auto result = Reverse(TBasicStringView<char>("a"));
        REQUIRE(result == "a");
    }

    SECTION("Two characters are swapped")
    {
        auto result = Reverse(TBasicStringView<char>("ab"));
        REQUIRE(result == "ba");
    }

    SECTION("Empty string returns empty")
    {
        auto result = Reverse(TBasicStringView<char>(""));
        REQUIRE(result.IsEmpty());
    }

    SECTION("String with digits and symbols is reversed correctly")
    {
        auto result = Reverse(TBasicStringView<char>("abc 123!"));
        REQUIRE(result == "!321 cba");
    }
}

TEST_CASE("IsAllAlpha", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All alphabetic string returns true")
    {
        REQUIRE(IsAllAlpha(TBasicStringView<char>("hello")));
        REQUIRE(IsAllAlpha(TBasicStringView<char>("hElLo")));
        REQUIRE(IsAllAlpha(TBasicStringView<char>("HELLO")));
    }

    SECTION("String with a digit returns false") { REQUIRE_FALSE(IsAllAlpha(TBasicStringView<char>("hello1"))); }

    SECTION("String with whitespace returns false")
    {
        REQUIRE_FALSE(IsAllAlpha(TBasicStringView<char>("hello world")));
    }

    SECTION("String with underscore returns false")
    {
        REQUIRE_FALSE(IsAllAlpha(TBasicStringView<char>("hello_world")));
    }

    SECTION("Empty string returns false") { REQUIRE_FALSE(IsAllAlpha(TBasicStringView<char>(""))); }

    SECTION("Single letter returns true") { REQUIRE(IsAllAlpha(TBasicStringView<char>("a"))); }
}

TEST_CASE("IsAllDigits", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All-digit string returns true")
    {
        REQUIRE(IsAllDigits(TBasicStringView<char>("12345")));
        REQUIRE(IsAllDigits(TBasicStringView<char>("0")));
    }

    SECTION("String containing a letter returns false") { REQUIRE_FALSE(IsAllDigits(TBasicStringView<char>("12a45"))); }

    SECTION("String with whitespace returns false") { REQUIRE_FALSE(IsAllDigits(TBasicStringView<char>("12 45"))); }

    SECTION("Empty string returns false") { REQUIRE_FALSE(IsAllDigits(TBasicStringView<char>(""))); }

    SECTION("Single digit returns true") { REQUIRE(IsAllDigits(TBasicStringView<char>("7"))); }
}

TEST_CASE("IsAllAlNum", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All-alphanumeric string returns true")
    {
        REQUIRE(IsAllAlNum(TBasicStringView<char>("hello123")));
        REQUIRE(IsAllAlNum(TBasicStringView<char>("ABC789")));
    }

    SECTION("String with a special character returns false")
    {
        REQUIRE_FALSE(IsAllAlNum(TBasicStringView<char>("hello!")));
        REQUIRE_FALSE(IsAllAlNum(TBasicStringView<char>("my_var")));
    }

    SECTION("String with whitespace returns false")
    {
        REQUIRE_FALSE(IsAllAlNum(TBasicStringView<char>("hello world")));
    }

    SECTION("Empty string returns false") { REQUIRE_FALSE(IsAllAlNum(TBasicStringView<char>(""))); }
}

TEST_CASE("IsAllWhitespace", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All-whitespace string returns true")
    {
        REQUIRE(IsAllWhitespace(TBasicStringView<char>("   ")));
        REQUIRE(IsAllWhitespace(TBasicStringView<char>(" \t\n\r")));
    }

    SECTION("String containing a non-whitespace character returns false")
    {
        REQUIRE_FALSE(IsAllWhitespace(TBasicStringView<char>("  a  ")));
    }

    SECTION("Empty string returns true per specification") { REQUIRE(IsAllWhitespace(TBasicStringView<char>(""))); }

    SECTION("Single whitespace character returns true") { REQUIRE(IsAllWhitespace(TBasicStringView<char>(" "))); }
}

TEST_CASE("IsAllUpper", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All-uppercase alphabetic string returns true")
    {
        REQUIRE(IsAllUpper(TBasicStringView<char>("HELLO")));
        REQUIRE(IsAllUpper(TBasicStringView<char>("ABC")));
    }

    SECTION("Mixed-case string returns false")
    {
        REQUIRE_FALSE(IsAllUpper(TBasicStringView<char>("Hello")));
        REQUIRE_FALSE(IsAllUpper(TBasicStringView<char>("HELLo")));
    }

    SECTION("Non-alphabetic characters are ignored in the check")
    {
        REQUIRE(IsAllUpper(TBasicStringView<char>("HELLO123!")));
        REQUIRE(IsAllUpper(TBasicStringView<char>("FOO_BAR")));
    }

    SECTION("String with only digits (no alphabetic chars) returns true")
    {
        REQUIRE(IsAllUpper(TBasicStringView<char>("123")));
    }

    SECTION("Empty string returns false") { REQUIRE_FALSE(IsAllUpper(TBasicStringView<char>(""))); }
}

TEST_CASE("IsAllLower", "[GP][Core][Utilities][StringUtilities]")
{
    SECTION("All-lowercase alphabetic string returns true")
    {
        REQUIRE(IsAllLower(TBasicStringView<char>("hello")));
        REQUIRE(IsAllLower(TBasicStringView<char>("abc")));
    }

    SECTION("Mixed-case string returns false")
    {
        REQUIRE_FALSE(IsAllLower(TBasicStringView<char>("Hello")));
        REQUIRE_FALSE(IsAllLower(TBasicStringView<char>("hELLO")));
    }

    SECTION("Non-alphabetic characters are ignored in the check")
    {
        REQUIRE(IsAllLower(TBasicStringView<char>("hello123!")));
        REQUIRE(IsAllLower(TBasicStringView<char>("foo_bar")));
    }

    SECTION("String with only digits (no alphabetic chars) returns true")
    {
        REQUIRE(IsAllLower(TBasicStringView<char>("123")));
    }

    SECTION("Empty string returns false") { REQUIRE_FALSE(IsAllLower(TBasicStringView<char>(""))); }
}
