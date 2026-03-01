// Copyright (c) - Graphical Playground. All rights reserved.

#include "Utilities/StringFormat.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

using namespace GP;
using namespace GP::Utilities;
using namespace GP::Container;

TEST_CASE("ParseFormatSpec - empty spec", "[GP][Core][Utilities][StringFormat]")
{
    auto spec = ParseFormatSpec(TBasicStringView<char>(""));
    REQUIRE(spec.fillChar == ' ');
    REQUIRE(spec.align == TFormatSpec<char>::Alignment::Default);
    REQUIRE(spec.sign == TFormatSpec<char>::Sign::Default);
    REQUIRE_FALSE(spec.altForm);
    REQUIRE_FALSE(spec.zeroPad);
    REQUIRE(spec.width == -1);
    REQUIRE(spec.precision == -1);
    REQUIRE(spec.type == TFormatSpec<char>::Presentation::Default);
}

TEST_CASE("ParseFormatSpec - alignment", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Left align '<'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("<"));
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Left);
    }

    SECTION("Right align '>'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>(">"));
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Right);
    }

    SECTION("Center align '^'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("^"));
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Center);
    }

    SECTION("Fill character with left align")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("*<"));
        REQUIRE(spec.fillChar == '*');
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Left);
    }

    SECTION("Fill character with right align")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("->"));
        REQUIRE(spec.fillChar == '-');
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Right);
    }

    SECTION("Fill character with center align")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("0^"));
        REQUIRE(spec.fillChar == '0');
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Center);
    }
}

TEST_CASE("ParseFormatSpec - sign", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Plus sign '+'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("+"));
        REQUIRE(spec.sign == TFormatSpec<char>::Sign::Plus);
    }

    SECTION("Minus sign '-'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("-"));
        REQUIRE(spec.sign == TFormatSpec<char>::Sign::Minus);
    }

    SECTION("Space sign ' '")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>(" "));
        REQUIRE(spec.sign == TFormatSpec<char>::Sign::Space);
    }
}

TEST_CASE("ParseFormatSpec - flags", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Alternate form '#'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("#"));
        REQUIRE(spec.altForm);
    }

    SECTION("Zero-pad '0'")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("0"));
        REQUIRE(spec.zeroPad);
    }
}

TEST_CASE("ParseFormatSpec - width", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Single-digit width")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("5"));
        REQUIRE(spec.width == 5);
    }

    SECTION("Multi-digit width")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("20"));
        REQUIRE(spec.width == 20);
    }

    SECTION("Width combined with align")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>(">10"));
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Right);
        REQUIRE(spec.width == 10);
    }
}

TEST_CASE("ParseFormatSpec - precision", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Precision only")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>(".3"));
        REQUIRE(spec.precision == 3);
    }

    SECTION("Width and precision")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("10.2"));
        REQUIRE(spec.width == 10);
        REQUIRE(spec.precision == 2);
    }

    SECTION("Zero precision")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>(".0"));
        REQUIRE(spec.precision == 0);
    }
}

TEST_CASE("ParseFormatSpec - presentation types", "[GP][Core][Utilities][StringFormat]")
{
    using P = TFormatSpec<char>::Presentation;

    SECTION("'b' maps to Binary") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("b")).type == P::Binary); }
    SECTION("'B' maps to BinaryUpper") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("B")).type == P::BinaryUpper); }
    SECTION("'o' maps to Octal") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("o")).type == P::Octal); }
    SECTION("'d' maps to Decimal") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("d")).type == P::Decimal); }
    SECTION("'x' maps to HexLower") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("x")).type == P::HexLower); }
    SECTION("'X' maps to HexUpper") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("X")).type == P::HexUpper); }
    SECTION("'f' maps to FloatFixed") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("f")).type == P::FloatFixed); }
    SECTION("'F' maps to FloatFixedUpper")
    {
        REQUIRE(ParseFormatSpec(TBasicStringView<char>("F")).type == P::FloatFixedUpper);
    }
    SECTION("'e' maps to FloatSci") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("e")).type == P::FloatSci); }
    SECTION("'E' maps to FloatSciUpper")
    {
        REQUIRE(ParseFormatSpec(TBasicStringView<char>("E")).type == P::FloatSciUpper);
    }
    SECTION("'g' maps to FloatGeneral")
    {
        REQUIRE(ParseFormatSpec(TBasicStringView<char>("g")).type == P::FloatGeneral);
    }
    SECTION("'G' maps to FloatGeneralUpper")
    {
        REQUIRE(ParseFormatSpec(TBasicStringView<char>("G")).type == P::FloatGeneralUpper);
    }
    SECTION("'s' maps to String") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("s")).type == P::String); }
    SECTION("'c' maps to Char") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("c")).type == P::Char); }
    SECTION("'p' maps to Pointer") { REQUIRE(ParseFormatSpec(TBasicStringView<char>("p")).type == P::Pointer); }
}

TEST_CASE("ParseFormatSpec - combined spec", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Full spec: fill, align, sign, alt, zero, width, precision, type")
    {
        // e.g. "*>+#010.3f"
        auto spec = ParseFormatSpec(TBasicStringView<char>("*>+#010.3f"));
        REQUIRE(spec.fillChar == '*');
        REQUIRE(spec.align == TFormatSpec<char>::Alignment::Right);
        REQUIRE(spec.sign == TFormatSpec<char>::Sign::Plus);
        REQUIRE(spec.altForm);
        REQUIRE(spec.zeroPad);
        REQUIRE(spec.width == 10);
        REQUIRE(spec.precision == 3);
        REQUIRE(spec.type == TFormatSpec<char>::Presentation::FloatFixed);
    }

    SECTION("Zero-pad with width and type: 08d")
    {
        auto spec = ParseFormatSpec(TBasicStringView<char>("08d"));
        REQUIRE(spec.zeroPad);
        REQUIRE(spec.width == 8);
        REQUIRE(spec.type == TFormatSpec<char>::Presentation::Decimal);
    }
}

TEST_CASE("Format - no arguments", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Plain string with no placeholders is returned as-is")
    {
        auto result = Format<char>("hello world");
        REQUIRE(result == "hello world");
    }

    SECTION("Empty format string returns empty string")
    {
        auto result = Format<char>("");
        REQUIRE(result.IsEmpty());
    }

    SECTION("Escaped opening brace '{{' becomes '{'")
    {
        auto result = Format<char>("{{");
        REQUIRE(result == "{");
    }

    SECTION("Escaped closing brace '}}' becomes '}'")
    {
        auto result = Format<char>("}}");
        REQUIRE(result == "}");
    }

    SECTION("Escaped braces surrounding literal text")
    {
        auto result = Format<char>("{{literal}}");
        REQUIRE(result == "{literal}");
    }
}

TEST_CASE("Format - bool", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("true formats as 'true' by default") { REQUIRE(Format<char>("{}", true) == "true"); }

    SECTION("false formats as 'false' by default") { REQUIRE(Format<char>("{}", false) == "false"); }

    SECTION("'d' presentation formats true as '1'") { REQUIRE(Format<char>("{:d}", true) == "1"); }

    SECTION("'d' presentation formats false as '0'") { REQUIRE(Format<char>("{:d}", false) == "0"); }

    SECTION("'x' presentation formats true as '1'") { REQUIRE(Format<char>("{:x}", true) == "1"); }

    SECTION("'b' presentation formats true as '1'") { REQUIRE(Format<char>("{:b}", true) == "1"); }

    SECTION("Width and left alignment applied to 'true'") { REQUIRE(Format<char>("{:<8}", true) == "true    "); }

    SECTION("Width and right alignment applied to 'false'") { REQUIRE(Format<char>("{:>8}", false) == "   false"); }
}

TEST_CASE("Format - char", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Single char is output as its character representation")
    {
        REQUIRE(Format<char>("{}", 'A') == "A");
        REQUIRE(Format<char>("{}", 'z') == "z");
        REQUIRE(Format<char>("{}", '!') == "!");
    }

    SECTION("Width with default left alignment for chars") { REQUIRE(Format<char>("{:5}", 'A') == "A    "); }

    SECTION("'d' presentation outputs the ASCII code point as decimal")
    {
        REQUIRE(Format<char>("{:d}", 'A') == "65");
        REQUIRE(Format<char>("{:d}", '0') == "48");
    }

    SECTION("'x' presentation outputs the code point as lowercase hex") { REQUIRE(Format<char>("{:x}", 'A') == "41"); }
}

TEST_CASE("Format - signed integer", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Default decimal representation")
    {
        REQUIRE(Format<char>("{}", 0) == "0");
        REQUIRE(Format<char>("{}", 42) == "42");
        REQUIRE(Format<char>("{}", -42) == "-42");
        REQUIRE(Format<char>("{}", 2147483647) == "2147483647");
        REQUIRE(Format<char>("{}", -2147483648) == "-2147483648");
    }

    SECTION("Explicit decimal 'd' presentation")
    {
        REQUIRE(Format<char>("{:d}", 255) == "255");
        REQUIRE(Format<char>("{:d}", -1) == "-1");
    }

    SECTION("Binary 'b' presentation")
    {
        REQUIRE(Format<char>("{:b}", 0) == "0");
        REQUIRE(Format<char>("{:b}", 1) == "1");
        REQUIRE(Format<char>("{:b}", 10) == "1010");
        REQUIRE(Format<char>("{:b}", 255) == "11111111");
    }

    SECTION("Binary 'b' with alt-form '#' adds '0b' prefix") { REQUIRE(Format<char>("{:#b}", 10) == "0b1010"); }

    SECTION("BinaryUpper 'B' with alt-form '#' adds '0B' prefix") { REQUIRE(Format<char>("{:#B}", 10) == "0B1010"); }

    SECTION("Octal 'o' presentation")
    {
        REQUIRE(Format<char>("{:o}", 0) == "0");
        REQUIRE(Format<char>("{:o}", 8) == "10");
        REQUIRE(Format<char>("{:o}", 255) == "377");
    }

    SECTION("Octal 'o' with alt-form '#' adds leading '0'") { REQUIRE(Format<char>("{:#o}", 8) == "010"); }

    SECTION("Octal alt-form of zero does not add extra '0'") { REQUIRE(Format<char>("{:#o}", 0) == "0"); }

    SECTION("Hex lowercase 'x' presentation")
    {
        REQUIRE(Format<char>("{:x}", 0) == "0");
        REQUIRE(Format<char>("{:x}", 255) == "ff");
        REQUIRE(Format<char>("{:x}", 256) == "100");
    }

    SECTION("Hex uppercase 'X' presentation")
    {
        REQUIRE(Format<char>("{:X}", 255) == "FF");
        REQUIRE(Format<char>("{:X}", 4096) == "1000");
    }

    SECTION("Hex 'x' with alt-form '#' adds '0x' prefix") { REQUIRE(Format<char>("{:#x}", 255) == "0xff"); }

    SECTION("Hex 'X' with alt-form '#' adds '0X' prefix") { REQUIRE(Format<char>("{:#X}", 255) == "0XFF"); }

    SECTION("Plus sign shows '+' for positive values")
    {
        REQUIRE(Format<char>("{:+d}", 42) == "+42");
        REQUIRE(Format<char>("{:+d}", -42) == "-42");
        REQUIRE(Format<char>("{:+d}", 0) == "+0");
    }

    SECTION("Space sign shows ' ' for positive values and '-' for negative")
    {
        REQUIRE(Format<char>("{: d}", 42) == " 42");
        REQUIRE(Format<char>("{: d}", -42) == "-42");
    }

    SECTION("Minus sign (explicit) shows '-' only for negative")
    {
        REQUIRE(Format<char>("{:-d}", 42) == "42");
        REQUIRE(Format<char>("{:-d}", -42) == "-42");
    }

    SECTION("Width right-aligns numeric values by default")
    {
        REQUIRE(Format<char>("{:5d}", 42) == "   42");
        REQUIRE(Format<char>("{:5d}", -42) == "  -42");
    }

    SECTION("Width left-aligns with '<'") { REQUIRE(Format<char>("{:<5d}", 42) == "42   "); }

    SECTION("Width center-aligns with '^'") { REQUIRE(Format<char>("{:^5d}", 42) == " 42  "); }

    SECTION("Custom fill character with alignment")
    {
        REQUIRE(Format<char>("{:*>5d}", 42) == "***42");
        REQUIRE(Format<char>("{:*<5d}", 42) == "42***");
        REQUIRE(Format<char>("{:*^5d}", 42) == "*42**");
    }

    SECTION("Zero-pad fills between sign/prefix and digits")
    {
        REQUIRE(Format<char>("{:08d}", 42) == "00000042");
        REQUIRE(Format<char>("{:08d}", -42) == "-0000042");
        REQUIRE(Format<char>("{:08x}", 255) == "000000ff");
    }

    SECTION("Zero-pad with alt-form") { REQUIRE(Format<char>("{:#010x}", 255) == "0x000000ff"); }

    SECTION("'c' presentation outputs as character")
    {
        REQUIRE(Format<char>("{:c}", 65) == "A");
        REQUIRE(Format<char>("{:c}", 97) == "a");
    }

    SECTION("Width equal to content length produces no padding") { REQUIRE(Format<char>("{:2d}", 42) == "42"); }

    SECTION("Width smaller than content does not truncate") { REQUIRE(Format<char>("{:1d}", 12345) == "12345"); }
}

TEST_CASE("Format - unsigned integer", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Default decimal representation")
    {
        REQUIRE(Format<char>("{}", UInt32(0)) == "0");
        REQUIRE(Format<char>("{}", UInt32(42)) == "42");
        REQUIRE(Format<char>("{}", UInt32(4294967295u)) == "4294967295");
    }

    SECTION("Hex lowercase")
    {
        REQUIRE(Format<char>("{:x}", UInt32(255)) == "ff");
        REQUIRE(Format<char>("{:#x}", UInt32(255)) == "0xff");
    }

    SECTION("Hex uppercase") { REQUIRE(Format<char>("{:X}", UInt32(255)) == "FF"); }

    SECTION("Binary") { REQUIRE(Format<char>("{:b}", UInt32(5)) == "101"); }

    SECTION("Octal") { REQUIRE(Format<char>("{:o}", UInt32(8)) == "10"); }

    SECTION("UInt64 large value")
    {
        REQUIRE(Format<char>("{}", UInt64(18446744073709551615ull)) == "18446744073709551615");
    }

    SECTION("Zero-pad") { REQUIRE(Format<char>("{:04d}", UInt32(7)) == "0007"); }
}

TEST_CASE("Format - float", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Default 'g' representation removes trailing zeros")
    {
        auto result = Format<char>("{}", 3.14f);
        REQUIRE_FALSE(result.IsEmpty());
        // Verify it round-trips via snprintf with %g
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%g", static_cast<double>(3.14f));
        REQUIRE(result == expected);
    }

    SECTION("Fixed 'f' with default precision is 6 fractional digits")
    {
        REQUIRE(Format<char>("{:f}", 3.0f) == "3.000000");
    }

    SECTION("Fixed 'f' with explicit precision")
    {
        REQUIRE(Format<char>("{:.2f}", 3.14159f) == "3.14");
        REQUIRE(Format<char>("{:.0f}", 3.7f) == "4");
        REQUIRE(Format<char>("{:.4f}", 1.0f) == "1.0000");
    }

    SECTION("Scientific 'e' presentation")
    {
        auto result = Format<char>("{:e}", 12345.0f);
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%e", static_cast<double>(12345.0f));
        REQUIRE(result == expected);
    }

    SECTION("Scientific 'E' uppercase presentation")
    {
        auto result = Format<char>("{:E}", 12345.0f);
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%E", static_cast<double>(12345.0f));
        REQUIRE(result == expected);
    }

    SECTION("Plus sign for positive float")
    {
        auto result = Format<char>("{:+.1f}", 3.1f);
        REQUIRE(result == "+3.1");
    }

    SECTION("Space sign for positive float")
    {
        auto result = Format<char>("{: .1f}", 3.1f);
        REQUIRE(result == " 3.1");
    }

    SECTION("Width right-aligns floats by default")
    {
        auto result = Format<char>("{:8.2f}", 3.14f);
        REQUIRE(result == "    3.14");
    }

    SECTION("Width left-aligns with '<'")
    {
        auto result = Format<char>("{:<8.2f}", 3.14f);
        REQUIRE(result == "3.14    ");
    }
}

TEST_CASE("Format - double", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Default representation")
    {
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%g", 3.14159265358979);
        REQUIRE(Format<char>("{}", 3.14159265358979) == expected);
    }

    SECTION("Fixed 'f' with precision 2") { REQUIRE(Format<char>("{:.2f}", 3.14159265358979) == "3.14"); }

    SECTION("Fixed 'f' with precision 6 (default)") { REQUIRE(Format<char>("{:f}", 1.0) == "1.000000"); }

    SECTION("Scientific 'e' with precision 3")
    {
        auto result = Format<char>("{:.3e}", 12345.6789);
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%.3e", 12345.6789);
        REQUIRE(result == expected);
    }

    SECTION("General 'g' with large value switches to scientific notation")
    {
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%g", 1e10);
        REQUIRE(Format<char>("{:g}", 1e10) == expected);
    }

    SECTION("General 'G' uppercase")
    {
        char expected[64];
        std::snprintf(expected, sizeof(expected), "%G", 1e10);
        REQUIRE(Format<char>("{:G}", 1e10) == expected);
    }

    SECTION("Negative double is formatted with leading '-'") { REQUIRE(Format<char>("{:.2f}", -3.14) == "-3.14"); }

    SECTION("Zero is formatted correctly") { REQUIRE(Format<char>("{:.2f}", 0.0) == "0.00"); }

    SECTION("Alt-form '#' retains decimal point for integer-valued float")
    {
        auto result = Format<char>("{:#.0f}", 3.0);
        REQUIRE(result == "3.");
    }
}

TEST_CASE("Format - const char*", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("C-string is output verbatim") { REQUIRE(Format<char>("{}", "hello") == "hello"); }

    SECTION("Null pointer formats as '(null)'")
    {
        const char* p = nullptr;
        REQUIRE(Format<char>("{}", p) == "(null)");
    }

    SECTION("Precision truncates the string to the given number of characters")
    {
        REQUIRE(Format<char>("{:.3}", "hello") == "hel");
        REQUIRE(Format<char>("{:.0}", "hello") == "");
    }

    SECTION("Width left-aligns string by default") { REQUIRE(Format<char>("{:10}", "hi") == "hi        "); }

    SECTION("Width with right alignment") { REQUIRE(Format<char>("{:>10}", "hi") == "        hi"); }

    SECTION("Width with center alignment") { REQUIRE(Format<char>("{:^10}", "hi") == "    hi    "); }

    SECTION("Width with custom fill and left alignment") { REQUIRE(Format<char>("{:-<10}", "hi") == "hi--------"); }

    SECTION("Precision and width combined: truncate then pad")
    {
        REQUIRE(Format<char>("{:10.3}", "hello") == "hel       ");
    }

    SECTION("Width smaller than content does not truncate") { REQUIRE(Format<char>("{:3}", "hello") == "hello"); }
}

TEST_CASE("Format - TBasicString", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("TBasicString value is output verbatim")
    {
        TBasicString<char> s("hello");
        REQUIRE(Format<char>("{}", s) == "hello");
    }

    SECTION("Precision truncates TBasicString")
    {
        TBasicString<char> s("hello");
        REQUIRE(Format<char>("{:.3}", s) == "hel");
    }

    SECTION("Width pads TBasicString on the right by default")
    {
        TBasicString<char> s("hi");
        REQUIRE(Format<char>("{:5}", s) == "hi   ");
    }
}

TEST_CASE("Format - TBasicStringView", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("TBasicStringView is output verbatim")
    {
        TBasicStringView<char> sv("hello");
        REQUIRE(Format<char>("{}", sv) == "hello");
    }

    SECTION("Precision truncates TBasicStringView")
    {
        TBasicStringView<char> sv("hello");
        REQUIRE(Format<char>("{:.3}", sv) == "hel");
    }
}

TEST_CASE("Format - void pointer", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Non-null void* is formatted as '0x…' with zero-padded hex digits")
    {
        // Use a known address by casting an integer to void*.
        void* p = reinterpret_cast<void*>(static_cast<UIntPtr>(0xDEADBEEF));
        auto result = Format<char>("{}", p);
        // The result must start with "0x".
        REQUIRE(result.Size() >= 2);
        REQUIRE(result[0] == '0');
        REQUIRE(result[1] == 'x');
        // The result must contain "deadbeef" (lowercase hex).
        // Find "deadbeef" in the output.
        TBasicStringView<char> sv(result);
        REQUIRE(sv.Find(TBasicStringView<char>("deadbeef")) != TBasicStringView<char>::NPOS);
    }

    SECTION("Null void* is formatted as a zero-filled hex address")
    {
        void* p = nullptr;
        auto result = Format<char>("{}", p);
        REQUIRE(result[0] == '0');
        REQUIRE(result[1] == 'x');
        // All remaining characters should be '0'.
        for (SizeT i = 2; i < result.Size(); ++i) { REQUIRE(result[i] == '0'); }
    }

    SECTION("nullptr formats the same as null void*")
    {
        auto ptrResult = Format<char>("{}", static_cast<void*>(nullptr));
        auto nullResult = Format<char>("{}", nullptr);
        REQUIRE(ptrResult == nullResult);
    }
}

TEST_CASE("Format - sequential argument substitution", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Two arguments in order") { REQUIRE(Format<char>("{} {}", "hello", "world") == "hello world"); }

    SECTION("Three arguments of different types")
    {
        REQUIRE(Format<char>("{} {} {}", 1, 2.0f, "three") == "1 2 three");
    }

    SECTION("Argument used with adjacent literal text") { REQUIRE(Format<char>("[{}]", 42) == "[42]"); }

    SECTION("Multiple placeholders, no literal text between them")
    {
        REQUIRE(Format<char>("{}{}{}", 'a', 'b', 'c') == "abc");
    }

    SECTION("Literal text only at the start") { REQUIRE(Format<char>("value={}", 99) == "value=99"); }

    SECTION("Literal text only at the end") { REQUIRE(Format<char>("{}!", "hi") == "hi!"); }

    SECTION("No placeholders returns format string verbatim")
    {
        REQUIRE(Format<char>("no placeholders") == "no placeholders");
    }
}

TEST_CASE("Format - positional argument substitution", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Arguments referenced in reverse order")
    {
        REQUIRE(Format<char>("{1} {0}", "world", "hello") == "hello world");
    }

    SECTION("Same argument referenced multiple times") { REQUIRE(Format<char>("{0} {0}", "x") == "x x"); }

    SECTION("Positional index 0") { REQUIRE(Format<char>("{0}", 42) == "42"); }

    SECTION("Positional with format spec") { REQUIRE(Format<char>("{0:08x}", 255) == "000000ff"); }

    SECTION("Multiple positional arguments in non-sequential order")
    {
        REQUIRE(Format<char>("{2}-{0}-{1}", "a", "b", "c") == "c-a-b");
    }
}

TEST_CASE("Format - escaped braces", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("'{{' produces a literal '{'") { REQUIRE(Format<char>("{{") == "{"); }

    SECTION("'}}' produces a literal '}'") { REQUIRE(Format<char>("}}") == "}"); }

    SECTION("Escaped braces surrounding a substituted value") { REQUIRE(Format<char>("{{{}}}", 42) == "{42}"); }

    SECTION("Escaped braces in the middle of plain text") { REQUIRE(Format<char>("a{{b}}c") == "a{b}c"); }

    SECTION("Only escaped braces, no arguments") { REQUIRE(Format<char>("{{}}") == "{}"); }
}

TEST_CASE("Format - alignment and fill", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Default alignment for strings is left") { REQUIRE(Format<char>("{:5}", "hi") == "hi   "); }

    SECTION("Default alignment for integers is right") { REQUIRE(Format<char>("{:5}", 42) == "   42"); }

    SECTION("Center alignment with odd padding places extra fill on the right")
    {
        REQUIRE(Format<char>("{:^5}", 42) == " 42  ");
    }

    SECTION("Fill character is applied correctly on all sides")
    {
        REQUIRE(Format<char>("{:0>5}", 42) == "00042");
        REQUIRE(Format<char>("{:0<5}", 42) == "42000");
        REQUIRE(Format<char>("{:0^5}", 42) == "04200");
    }

    SECTION("Width equal to content length produces no padding") { REQUIRE(Format<char>("{:2}", 42) == "42"); }

    SECTION("Width smaller than content does not truncate integers")
    {
        REQUIRE(Format<char>("{:1}", 12345) == "12345");
    }
}

TEST_CASE("Format - sign formatting", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Plus sign with zero") { REQUIRE(Format<char>("{:+d}", 0) == "+0"); }

    SECTION("Space sign with negative value shows '-' not ' '") { REQUIRE(Format<char>("{: d}", -1) == "-1"); }

    SECTION("Plus sign with hex") { REQUIRE(Format<char>("{:+x}", 255) == "+ff"); }

    SECTION("Alt-form hex with zero value") { REQUIRE(Format<char>("{:#x}", 0) == "0x0"); }

    SECTION("Alt-form binary with zero value") { REQUIRE(Format<char>("{:#b}", 0) == "0b0"); }
}

TEST_CASE("Format - TFormatArg type dispatch", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Int32 dispatched correctly")
    {
        Int32 v = -100;
        REQUIRE(Format<char>("{}", v) == "-100");
    }

    SECTION("Int64 dispatched correctly")
    {
        Int64 v = -9223372036854775807LL;
        REQUIRE(Format<char>("{:d}", v) == "-9223372036854775807");
    }

    SECTION("UInt32 dispatched correctly")
    {
        UInt32 v = 4000000000u;
        REQUIRE(Format<char>("{}", v) == "4000000000");
    }

    SECTION("UInt64 dispatched correctly")
    {
        UInt64 v = 10000000000ull;
        REQUIRE(Format<char>("{}", v) == "10000000000");
    }

    SECTION("Float32 dispatched correctly")
    {
        Float32 v = 1.5f;
        REQUIRE(Format<char>("{:.1f}", v) == "1.5");
    }

    SECTION("Float64 dispatched correctly")
    {
        Float64 v = 1.5;
        REQUIRE(Format<char>("{:.1f}", v) == "1.5");
    }
}

TEST_CASE("Format - mixed literal and placeholder", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Multiple types in one format string")
    {
        auto result = Format<char>("int={} float={:.2f} str={} bool={}", 42, 3.14, "hello", true);
        REQUIRE(result == "int=42 float=3.14 str=hello bool=true");
    }

    SECTION("Consecutive placeholders with no delimiter") { REQUIRE(Format<char>("{}{}{}", 1, 2, 3) == "123"); }

    SECTION("Escaped braces and substitution mixed")
    {
        REQUIRE(Format<char>("{{{}}} = {}", "key", 100) == "{key} = 100");
    }

    SECTION("C-string format with null-terminated overload")
    {
        const char* fmt = "x={}, y={}";
        auto result = Format<char>(fmt, 10, 20);
        REQUIRE(result == "x=10, y=20");
    }

    SECTION("Large number of arguments produces correct output")
    {
        REQUIRE(Format<char>("{} {} {} {} {}", 1, 2, 3, 4, 5) == "1 2 3 4 5");
    }

    SECTION("Integer min value (INT32_MIN) handled without UB")
    {
        Int32 v = -2147483648;
        REQUIRE(Format<char>("{}", v) == "-2147483648");
    }

    SECTION("Integer min value (INT64_MIN) handled without UB")
    {
        Int64 v = (-9223372036854775807LL - 1LL);
        REQUIRE(Format<char>("{}", v) == "-9223372036854775808");
    }
}

TEST_CASE("Format - zero-pad edge cases", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Zero-pad with positive integer and '+' sign") { REQUIRE(Format<char>("{:+08d}", 42) == "+0000042"); }

    SECTION("Zero-pad with negative integer") { REQUIRE(Format<char>("{:08d}", -42) == "-0000042"); }

    SECTION("Zero-pad with hex and alt-form: prefix then zeros then digits")
    {
        REQUIRE(Format<char>("{:#010x}", 255) == "0x000000ff");
    }

    SECTION("Zero-pad width smaller than content produces no padding")
    {
        REQUIRE(Format<char>("{:03d}", 12345) == "12345");
    }
}

TEST_CASE("Format - string precision edge cases", "[GP][Core][Utilities][StringFormat]")
{
    SECTION("Precision larger than string length does not truncate") { REQUIRE(Format<char>("{:.10}", "hi") == "hi"); }

    SECTION("Precision of zero produces empty string from content") { REQUIRE(Format<char>("{:.0}", "hello") == ""); }

    SECTION("Precision equal to string length keeps full string")
    {
        REQUIRE(Format<char>("{:.5}", "hello") == "hello");
    }

    SECTION("Precision combined with width pads after truncation")
    {
        REQUIRE(Format<char>("{:8.3}", "hello") == "hel     ");
    }

    SECTION("Precision 0 with width pads empty content") { REQUIRE(Format<char>("{:5.0}", "hello") == "     "); }
}
