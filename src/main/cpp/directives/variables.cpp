#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

namespace {

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

long long operate(Expression_Type type, long long x, long long y)
{
    switch (type) {
    case Expression_Type::add: return x + y;
    case Expression_Type::subtract: return x - y;
    case Expression_Type::multiply: return x * y;
    case Expression_Type::divide: return x / y;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

} // namespace

Content_Status
Expression_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    long long result = expression_type_neutral_element(m_type);
    for (const ast::Argument& arg : d.get_arguments()) {
        std::pmr::vector<char8_t> arg_text { context.get_transient_memory() };
        const auto arg_status = to_plaintext(arg_text, arg.get_content(), context);
        if (arg_status != Content_Status::ok) {
            return arg_status;
        }
        const auto arg_string = as_u8string_view(arg_text);

        const std::optional x = from_chars<long long>(arg_string);
        if (!x) {
            const std::u8string_view message[] {
                u8"Unable to perform operation because \"",
                arg_string,
                u8"\" is not a valid integer.",
            };
            context.try_error(
                diagnostic::arithmetic_parse, arg.get_source_span(), joined_char_sequence(message)
            );
            return try_generate_error(out, d, context);
        }
        if (m_type == Expression_Type::divide && *x == 0) {
            const std::u8string_view message[] {
                u8"The dividend \"",
                arg_string,
                u8"\" evaluated to zero, and a division by zero would occur.",
            };
            context.try_error(
                diagnostic::arithmetic_div_by_zero, arg.get_source_span(),
                joined_char_sequence(message)
            );
            return try_generate_error(out, d, context);
        }
        result = operate(m_type, result, *x);
    }

    try_enter_paragraph(out);

    const Characters8 result_chars = to_characters8(result);
    out.write(result_chars.as_string(), Output_Language::text);
    return Content_Status::ok;
}

Content_Status
Variable_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    if (argument_to_plaintext(data, d, args, var_parameter, context)) {
        return generate_var(out, d, as_u8string_view(data), context);
    }
    return Content_Status::error;
}

Content_Status Get_Variable_Behavior::generate_var(
    Content_Policy& out,
    const ast::Directive&,
    std::u8string_view var,
    Context& context
) const
{
    try_enter_paragraph(out);

    const auto it = context.get_variables().find(var);
    if (it != context.get_variables().end()) {
        out.write(std::u8string_view { it->second }, Output_Language::text);
    }
    return Content_Status::ok;
}

Content_Status set_variable_to_op_result(
    Variable_Operation op,
    const ast::Directive& d,
    std::u8string_view var,
    Context& context
)
{
    std::pmr::vector<char8_t> body_string { context.get_transient_memory() };
    const auto status = to_plaintext(body_string, d.get_content(), context);
    if (status != Content_Status::ok) {
        return status;
    }

    const auto it = context.get_variables().find(var);
    if (op == Variable_Operation::set) {
        std::pmr::u8string value { body_string.data(), body_string.size(),
                                   context.get_persistent_memory() };
        if (it == context.get_variables().end()) {
            std::pmr::u8string key // NOLINT(misc-const-correctness)
                { var.data(), var.size(), context.get_persistent_memory() };
            context.get_variables().emplace(std::move(key), std::move(value));
        }
        else {
            it->second = std::move(value);
        }
    }
    return Content_Status::ok;
}

} // namespace cowel
