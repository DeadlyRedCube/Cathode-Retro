#pragma once

#pragma region Internals

// Nil/Identity give you back either nothing or exactly what you gave in.
#define MC_NIL(...)
#define MC_IDENTITY(...) __VA_ARGS__

// MC_STRINGIFY converts the arg to a string.
#define MC_STRINGIFY(...) MC_OBSTRUCT(MC_STRINGIFY_)(__VA_ARGS__)
#define MC_STRINGIFY_(...) #__VA_ARGS__

// CAT concatenates two things (with expansion)
#define MC_CAT_(a, ...) a ## __VA_ARGS__
#define MC_CAT(a, ...) MC_CAT_(a, ...)

// COMPLEMENT flips a 0 to a 1 and vice versa.
#define MC_COMPLEMENT(b) MC_CAT_(MC_COMPLEMENT_, b)
#define MC_COMPLEMENT_0 1
#define MC_COMPLEMENT_1 0

// MC_CHECK is complicated. Basically if you hand MC_CHECK(x) is 0, and MC_CHECK(MC_PROBE(x)) is 1.
#define MC_CHECK_N(x, n, ...) n
#define MC_CHECK(...) MC_DEFER(MC_CHECK_N)(__VA_ARGS__, 0,)
#define MC_PROBE(x) x, 1,

// NOT uses CHECK and PROBE to return 1 when x == 0, and 0 at any other time.
#define MC_NOT(x) MC_CHECK(MC_CAT_(MC_NOT_, x))
#define MC_NOT_0 MC_PROBE(~)

// BOOL uses MC_COMPLEMENT and MC_
#define MC_BOOL(x) MC_COMPLEMENT(MC_NOT(x))

// IF converts x to a bool 1 or 0, then switches on it. Usage is IF(condition)(true, false)
#define MC_IF_(cond) MC_CAT_(MC_IF_, cond)
#define MC_IF_0(t, ...) __VA_ARGS__
#define MC_IF_1(t, ...) t
#define MC_IF(x) MC_IF_(MC_BOOL(x))

// BITAND ands two values. MC_BITAND(x)(y)
#define MC_BITAND(x) MC_CAT_(MC_BITAND_, x)
#define MC_BITAND_0(y) 0
#define MC_BITAND_1(y) y

// BITOR ors two values. MC_BITOR(x)(y)
#define MC_BITOR(x) MC_CAT_(MC_BITOR_, x)
#define MC_BITOR_0(y) y
#define MC_BITOR_1(y) 1

// MC_HAS_ARGS is 1 when there are arguments and 0 when there are no arguments.
#define MC_HAS_ARGS_SECOND_PARAM(a, b, ...) b
#define MC_HAS_ARGS_MC_NO_ARGS MC_PROBE(~)
#define MC_HAS_ARGS(...) MC_COMPLEMENT(MC_CHECK(MC_CAT_(MC_HAS_ARGS_, MC_HAS_ARGS_SECOND_PARAM(~, __VA_ARGS__ MC_NO_ARGS))))

// WHEN expands to IDENTITY when x is true and NIL when false. so WHEN(condition)(thing) is either "thing" or ""
#define MC_WHEN(x) MC_IF(x)(MC_IDENTITY, MC_NIL)

// MC_IS_TERM checks for MC_TERM. thus, MC_IS_TERM(MC_TERM) is 1 and MC_IS_TERM(anything else) is 0.
#define MC_IS_TERM(x) MC_CHECK(MC_CAT_(MC_IS_TERM_, x))
#define MC_IS_TERM_MC_TERM MC_PROBE(~)
#define MC_NOT_TERM(x) MC_NOT(MC_IS_TERM(x))

// MC_DEFER defers evaluation of a macro one preprocess step. MC_DEFER(macro)(macro args)
#define MC_EMPTY()
#define MC_DEFER(...) __VA_ARGS__ MC_EMPTY()

// MC_OBSTRUCT is a double defer.
#define MC_OBSTRUCT(...) __VA_ARGS__ MC_DEFER(MC_EMPTY)()

// MC_EVAL adds more scans to the expression which allows a measure of recursion (due to the way the preprocessor handles disabling)
#define MC_EVAL(...)  MC_EVAL1(MC_EVAL1(MC_EVAL1(__VA_ARGS__)))
#define MC_EVAL1(...) MC_EVAL2(MC_EVAL2(MC_EVAL2(__VA_ARGS__)))
#define MC_EVAL2(...) MC_EVAL3(MC_EVAL3(MC_EVAL3(__VA_ARGS__)))
#define MC_EVAL3(...) MC_EVAL4(MC_EVAL4(MC_EVAL4(__VA_ARGS__)))
#define MC_EVAL4(...) MC_EVAL5(MC_EVAL5(MC_EVAL5(__VA_ARGS__)))
#define MC_EVAL5(...) __VA_ARGS__

// FOR_EACH evaluates the given macro as macro(param) for each __VA_ARG__ param. It does this by adding an MC_TERM to the end of the arg list, and then
//  looping when "first" is not  MC_TERM.
#define MC_FOR_EACH__() MC_FOR_EACH_
#define MC_FOR_EACH_(macro, first, ...) \
        MC_WHEN(MC_NOT_TERM(first)) \
        ( \
            MC_OBSTRUCT(macro) \
            ( \
                first \
            ) \
            MC_OBSTRUCT(MC_FOR_EACH__)() \
            ( \
                macro, __VA_ARGS__ \
            ) \
        )
#define MC_FOR_EACH(macro, ...)  MC_EVAL(MC_FOR_EACH_(macro, __VA_ARGS__, MC_TERM, ~))

// MC_STRIP_PARENS will optionally strip outer parentheses if they are there.
#define MC_HAS_OUTER_PARENS(x) MC_CHECK(MC_HAS_OUTER_PARENS_PROBE x)
#define MC_HAS_OUTER_PARENS_PROBE(...) MC_PROBE(~)
#define MC_STRIP_PARENS(x) MC_IF(MC_HAS_OUTER_PARENS(x))(MC_IDENTITY x, x)
#pragma endregion
