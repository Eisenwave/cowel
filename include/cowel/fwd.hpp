#ifndef COWEL_CONFIG_HPP
#define COWEL_CONFIG_HPP

#ifndef NDEBUG // debug builds
#define COWEL_DEBUG 1
#define COWEL_IF_DEBUG(...) __VA_ARGS__
#define COWEL_IF_NOT_DEBUG(...)
#else // release builds
#define COWEL_IF_DEBUG(...)
#define COWEL_IF_NOT_DEBUG(...) __VA_ARGS__
#endif

#ifdef __EMSCRIPTEN__
#define COWEL_EMSCRIPTEN 1
#define COWEL_IF_EMSCRIPTEN(...) __VA_ARGS__
#else
#define COWEL_IF_EMSCRIPTEN(...)
#endif

#ifdef __clang__
#define COWEL_CLANG 1
#endif

#define COWEL_UNREACHABLE() __builtin_unreachable()

namespace cowel {

/// @brief The default underlying type for scoped enumerations.
using Default_Underlying = unsigned char;

#define COWEL_ENUM_STRING_CASE(...)                                                                \
    case __VA_ARGS__: return #__VA_ARGS__

#define COWEL_ENUM_STRING_CASE8(...)                                                               \
    case __VA_ARGS__: return u8## #__VA_ARGS__

struct Annotated_String_Length;
template <typename>
struct Annotation_Span;
enum struct Diagnostic_Highlight : Default_Underlying;
struct Argument_Matcher;
enum struct Argument_Status : Default_Underlying;
struct AST_Formatting_Options;
struct AST_Instruction;
enum struct AST_Instruction_Type : Default_Underlying;
enum struct Attribute_Style : Default_Underlying;
struct Author_Info;
struct Builtin_Directive_Set;
template <typename, typename>
struct Basic_Annotated_String;
template <typename File>
struct Basic_File_Source_Position;
template <typename File>
struct Basic_File_Source_Span;
template <typename>
struct Basic_Transparent_String_View_Equals;
template <typename>
struct Basic_Transparent_String_View_Greater;
template <typename>
struct Basic_Transparent_String_View_Hash;
template <typename>
struct Basic_Transparent_String_View_Less;
enum struct Diagnostic_Highlight : Default_Underlying;
struct Content_Policy;
struct Context;
struct Diagnostic;
struct Bibliography;
struct Document_Info;
struct Document_Sections;
struct Directive_Behavior;
struct Error_Tag;
using File_Id = int;
struct Generation_Options;
enum struct HLJS_Scope : Default_Underlying;
struct Ignorant_Logger;
enum struct IO_Error_Code : Default_Underlying;
struct Logger;
struct Name_Resolver;
struct Simple_Bibliography;
struct No_Support_Syntax_Highlighter;
template <typename, typename>
struct Result;
enum struct Severity : Default_Underlying;
enum struct Sign_Policy : Default_Underlying;
struct Source_Position;
struct Source_Span;
struct Success_Tag;
struct Syntax_Highlighter;
enum struct Syntax_Highlight_Error : Default_Underlying;
enum struct To_HTML_Mode : Default_Underlying;

using File_Source_Position = Basic_File_Source_Position<File_Id>;
using File_Source_Span = Basic_File_Source_Span<File_Id>;

namespace ast {

struct Argument;
struct Content;
struct Directive;
struct Escaped;
struct Generated;
struct Text;

} // namespace ast

template <typename T>
using Annotated_String = Basic_Annotated_String<char, T>;
template <typename T>
using Annotated_String8 = Basic_Annotated_String<char8_t, T>;

using Diagnostic_String = Annotated_String8<Diagnostic_Highlight>;

using HLJS_Annotation_Span = Annotation_Span<HLJS_Scope>;

using Transparent_String_View_Equals = Basic_Transparent_String_View_Equals<char>;
using Transparent_String_View_Equals8 = Basic_Transparent_String_View_Equals<char8_t>;
using Transparent_String_View_Greater = Basic_Transparent_String_View_Greater<char>;
using Transparent_String_View_Greater8 = Basic_Transparent_String_View_Greater<char8_t>;
using Transparent_String_View_Hash = Basic_Transparent_String_View_Hash<char>;
using Transparent_String_View_Hash8 = Basic_Transparent_String_View_Hash<char8_t>;
using Transparent_String_View_Less = Basic_Transparent_String_View_Less<char>;
using Transparent_String_View_Less8 = Basic_Transparent_String_View_Less<char8_t>;

} // namespace cowel

#endif
