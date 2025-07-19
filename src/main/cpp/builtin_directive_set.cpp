#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/typo.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct Builtin_Directive_Set::Impl {
    Fixed_Name_Passthrough_Behavior b //
        { u8"b", Policy_Usage::inherit, Directive_Display::in_line };
    Special_Block_Behavior Babstract //
        { u8"abstract-block", Intro_Policy::yes };
    Special_Block_Behavior Bdecision //
        { u8"decision-block", Intro_Policy::yes };
    Special_Block_Behavior Bdel //
        { u8"del-block", Intro_Policy::no };
    Fixed_Name_Passthrough_Behavior Bdetails //
        { u8"details", Policy_Usage::inherit, Directive_Display::block };
    Special_Block_Behavior Bdiff //
        { u8"diff-block", Intro_Policy::no };
    Special_Block_Behavior Bex //
        { u8"example-block", Intro_Policy::yes };
    Bibliography_Add_Behavior bib //
        {};
    Special_Block_Behavior Bimp //
        { u8"important-block", Intro_Policy::yes };
    In_Tag_Behavior Bindent //
        { u8"div", u8"indent", Policy_Usage::html, Directive_Display::block };
    Special_Block_Behavior Bins //
        { u8"ins-block", Intro_Policy::no };
    HTML_Wrapper_Behavior block //
        { Directive_Display::block, To_HTML_Mode::direct };
    Special_Block_Behavior blockquote //
        { u8"blockquote", Intro_Policy::no };
    Special_Block_Behavior Bnote //
        { u8"note-block", Intro_Policy::yes };
    Special_Block_Behavior Bquote //
        { u8"blockquote", Intro_Policy::no };
    Self_Closing_Behavior br //
        { u8"br", Directive_Display::in_line };
    Special_Block_Behavior Btip //
        { u8"tip-block", Intro_Policy::yes };
    Special_Block_Behavior Btodo //
        { u8"todo-block", Intro_Policy::yes };
    Special_Block_Behavior Bug //
        { u8"bug-block", Intro_Policy::yes };
    Special_Block_Behavior Bwarn //
        { u8"warning-block", Intro_Policy::yes };
    Char_By_Entity_Behavior c //
        {};
    Expression_Behavior Cadd //
        { Expression_Type::add };
    Expression_Behavior Cdiv //
        { Expression_Type::divide };
    Expression_Behavior Cmul //
        { Expression_Type::multiply };
    Expression_Behavior Csub //
        { Expression_Type::subtract };
    Fixed_Name_Passthrough_Behavior caption //
        { u8"caption", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior cite //
        { u8"cite", Policy_Usage::html, Directive_Display::in_line };
    Code_Behavior code //
        { u8"code", Directive_Display::in_line, Pre_Trimming::no };
    Code_Behavior codeblock //
        { u8"code-block", Directive_Display::block, Pre_Trimming::yes };
    Fixed_Name_Passthrough_Behavior col //
        { u8"col", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior colgroup //
        { u8"colgroup", Policy_Usage::html, Directive_Display::block };
    Comment_Behavior comment //
        {};
    Fixed_Name_Passthrough_Behavior dd //
        { u8"dd", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior del //
        { u8"del", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior details //
        { u8"details", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior dfn //
        { u8"dfn", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior div //
        { u8"div", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior dl //
        { u8"dl", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior dt //
        { u8"dt", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior em //
        { u8"em", Policy_Usage::inherit, Directive_Display::in_line };
    Error_Behavior error //
        {};
    Fixed_Name_Passthrough_Behavior gterm //
        { u8"g-term", Policy_Usage::inherit, Directive_Display::in_line };
    Heading_Behavior h1 //
        { 1 };
    Heading_Behavior h2 //
        { 2 };
    Heading_Behavior h3 //
        { 3 };
    Heading_Behavior h4 //
        { 4 };
    Heading_Behavior h5 //
        { 5 };
    Heading_Behavior h6 //
        { 6 };
    Here_Behavior here //
        { Directive_Display::in_line };
    Here_Behavior hereblock //
        { Directive_Display::block };
    Highlight_As_Behavior hl // NOLINT(misc-confusable-identifiers)
        {};
    Self_Closing_Behavior hr //
        { u8"hr", Directive_Display::block };
    HTML_Behavior html //
        { Directive_Display::in_line };
    HTML_Behavior htmlblock //
        { Directive_Display::block };
    Directive_Name_Passthrough_Behavior html_tags //
        { Policy_Usage::html, Directive_Display::block, html_tag_prefix };
    Fixed_Name_Passthrough_Behavior i //
        { u8"i", Policy_Usage::inherit, Directive_Display::in_line };
    Import_Behavior import //
        {};
    Include_Behavior //
        include {};
    HTML_Wrapper_Behavior in_line //
        { Directive_Display::in_line, To_HTML_Mode::direct };
    Fixed_Name_Passthrough_Behavior ins //
        { u8"ins", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior kbd //
        { u8"kbd", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior li //
        { u8"li", Policy_Usage::html, Directive_Display::block };
    Literally_Behavior literally //
        {};
    Lorem_Ipsum_Behavior lorem_ipsum //
        {};
    Macro_Define_Behavior macro //
        {};
    Macro_Instantiate_Behavior macro_instantiate //
        {};
    URL_Behavior mail //
        { u8"mailto:" };
    Make_Section_Behavior make_bib //
        { Directive_Display::block, class_name::bibliography, section_name::bibliography };
    Make_Section_Behavior make_contents //
        { Directive_Display::block, class_name::table_of_contents,
          section_name::table_of_contents };
    Fixed_Name_Passthrough_Behavior mark //
        { u8"mark", Policy_Usage::inherit, Directive_Display::in_line };
    Math_Behavior math //
        { Directive_Display::in_line };
    Math_Behavior mathblock //
        { Directive_Display::block };
    Char_By_Name_Behavior N //
        {};
    In_Tag_Behavior nobr //
        { u8"span", u8"word", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior noscript //
        { u8"noscript", Policy_Usage::html, Directive_Display::block };
    In_Tag_Behavior o //
        { u8"span", u8"oblique", Policy_Usage::inherit, Directive_Display::in_line };
    List_Behavior ol //
        { u8"ol", li };
    Fixed_Name_Passthrough_Behavior p //
        { u8"p", Policy_Usage::html, Directive_Display::block };
    HTML_Wrapper_Behavior paragraphs //
        { Directive_Display::block, To_HTML_Mode::paragraphs };
    Fixed_Name_Passthrough_Behavior pre //
        { u8"pre", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior q //
        { u8"q", Policy_Usage::inherit, Directive_Display::in_line };
    Ref_Behavior ref //
        {};
    Fixed_Name_Passthrough_Behavior s //
        { u8"s", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior samp //
        { u8"samp", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior sans //
        { u8"f-sans", Policy_Usage::inherit, Directive_Display::in_line };
    HTML_Raw_Text_Behavior script //
        { u8"script" };
    Fixed_Name_Passthrough_Behavior serif //
        { u8"f-serif", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior small //
        { u8"small", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior span //
        { u8"span", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior strong //
        { u8"strong", Policy_Usage::inherit, Directive_Display::in_line };
    HTML_Raw_Text_Behavior style //
        { u8"style" };
    Fixed_Name_Passthrough_Behavior sub //
        { u8"sub", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior summary //
        { u8"summary", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior sup //
        { u8"sup", Policy_Usage::inherit, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior table //
        { u8"table", Policy_Usage::html, Directive_Display::block };
    URL_Behavior tel //
        { u8"tel:" };
    Plaintext_Wrapper_Behavior text //
        { Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior tbody //
        { u8"tbody", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior td //
        { u8"td", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior tfoot //
        { u8"tfoot", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior th //
        { u8"th", Policy_Usage::html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior thead //
        { u8"thead", Policy_Usage::html, Directive_Display::block };
    There_Behavior there //
        {};
    Fixed_Name_Passthrough_Behavior tr //
        { u8"tr", Policy_Usage::html, Directive_Display::block };
    Trim_Behavior trim //
        { Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior tt //
        { u8"tt-", Policy_Usage::inherit, Directive_Display::in_line };
    Char_By_Num_Behavior U //
        {};
    Fixed_Name_Passthrough_Behavior u //
        { u8"u", Policy_Usage::inherit, Directive_Display::in_line };
    Char_Get_Num_Behavior Udigits //
        {};
    List_Behavior ul //
        { u8"ul", li };
    Unprocessed_Behavior unprocessed //
        {};
    URL_Behavior url //
        {};
    Fixed_Name_Passthrough_Behavior var //
        { u8"var", Policy_Usage::inherit, Directive_Display::in_line };
    Get_Variable_Behavior Vget //
        {};
    Modify_Variable_Behavior Vset //
        { Variable_Operation::set };
    Self_Closing_Behavior wbr //
        { u8"wbr", Directive_Display::in_line };
    WG21_Block_Behavior wg21_example //
        { u8"Example", u8"end example" };
    In_Tag_Behavior wg21_grammar //
        { u8"dl", u8"grammar", Policy_Usage::html, Directive_Display::block };
    WG21_Head_Behavior wg21_head //
        {};
    WG21_Block_Behavior wg21_note //
        { u8"Note", u8"end note" };

    // clang-format off
#define COWEL_DEPRECATED_ALIAS(name, use_instead)                                                  \
    Deprecated_Behavior name { use_instead, u8## #use_instead }
    // clang-format on

    COWEL_DEPRECATED_ALIAS(abstract, Babstract);
    COWEL_DEPRECATED_ALIAS(bug, Bug);
    COWEL_DEPRECATED_ALIAS(decision, Bdecision);
    COWEL_DEPRECATED_ALIAS(delblock, Bdel);
    COWEL_DEPRECATED_ALIAS(diff, Bdiff);
    COWEL_DEPRECATED_ALIAS(example, Bex);
    COWEL_DEPRECATED_ALIAS(indent, Bindent);
    COWEL_DEPRECATED_ALIAS(important, Bimp);
    COWEL_DEPRECATED_ALIAS(insblock, Bins);
    COWEL_DEPRECATED_ALIAS(note, Bnote);
    COWEL_DEPRECATED_ALIAS(tip, Btip);
    COWEL_DEPRECATED_ALIAS(todo, Btodo);
    COWEL_DEPRECATED_ALIAS(warning, Bwarn);
    COWEL_DEPRECATED_ALIAS(word, nobr);

    COWEL_DEPRECATED_ALIAS(hyphen_make_bib, make_bib);
    COWEL_DEPRECATED_ALIAS(hyphen_make_contents, make_contents);
    COWEL_DEPRECATED_ALIAS(hyphen_lorem_ipsum, lorem_ipsum);
    COWEL_DEPRECATED_ALIAS(hyphen_wg21_example, wg21_example);
    COWEL_DEPRECATED_ALIAS(hyphen_wg21_grammar, wg21_grammar);
    COWEL_DEPRECATED_ALIAS(hyphen_wg21_head, wg21_head);
    COWEL_DEPRECATED_ALIAS(hyphen_wg21_note, wg21_note);

    Impl() = default;
    ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

Builtin_Directive_Set::Builtin_Directive_Set()
    : m_impl(std::make_unique<Builtin_Directive_Set::Impl>())
{
}

Builtin_Directive_Set::~Builtin_Directive_Set() = default;

Directive_Behavior& Builtin_Directive_Set::get_error_behavior() noexcept
{
    return m_impl->error;
}

Directive_Behavior& Builtin_Directive_Set::get_macro_behavior() noexcept
{
    return m_impl->macro_instantiate;
}

Distant<std::u8string_view>
Builtin_Directive_Set::fuzzy_lookup_name(std::u8string_view name, Context& context) const
{
    // clang-format off
    static constexpr std::u8string_view prefixed_names[] {
        u8"-Babstract",
        u8"-Bdecision",
        u8"-Bdel",
        u8"-Bdetails",
        u8"-Bdiff",
        u8"-Bex",
        u8"-Bimp",
        u8"-Bindent",
        u8"-Bins",
        u8"-Bnote",
        u8"-Bquote",
        u8"-Btip",
        u8"-Btodo",
        u8"-Bug",
        u8"-Bwarn",
        u8"-Cadd",
        u8"-Cdiv",
        u8"-Cmul",
        u8"-Csub",
        u8"-N",
        u8"-U",
        u8"-Udigits",
        u8"-Vget",
        u8"-Vset",
        u8"-abstract",
        u8"-b",
        u8"-bib",
        u8"-block",
        u8"-blockquote",
        u8"-br",
        u8"-bug",
        u8"-c",
        u8"-caption",
        u8"-cite",
        u8"-code",
        u8"-codeblock",
        u8"-comment",
        u8"-dd",
        u8"-decision",
        u8"-del",
        u8"-delblock",
        u8"-details",
        u8"-dfn",
        u8"-diff",
        u8"-div",
        u8"-dl",
        u8"-dt",
        u8"-em",
        u8"-error",
        u8"-example",
        u8"-gterm",
        u8"-h1",
        u8"-h2",
        u8"-h3",
        u8"-h4",
        u8"-h5",
        u8"-h6",
        u8"-here",
        u8"-hereblock",
        u8"-hl",
        u8"-hr",
        u8"-html",
        u8"-html-",
        u8"-htmlblock",
        u8"-i",
        u8"-import",
        u8"-important",
        u8"-indent",
        u8"-inline",
        u8"-ins",
        u8"-insblock",
        u8"-item",
        u8"-k",
        u8"-kbd",
        u8"-li",
        u8"-literally",
        u8"-lorem-ipsum",
        u8"-lorem_ipsum",
        u8"-macro",
        u8"-mail",
        u8"-make-bib",
        u8"-make-contents",
        u8"-make_bib",
        u8"-make_contents",
        u8"-mark",
        u8"-math",
        u8"-mathblock",
        u8"-nobr",
        u8"-noscript",
        u8"-note",
        u8"-ol",
        u8"-p",
        u8"-paragraphs",
        u8"-pre",
        u8"-q",
        u8"-ref",
        u8"-s",
        u8"-samp",
        u8"-sans",
        u8"-script",
        u8"-serif",
        u8"-small",
        u8"-span",
        u8"-strong",
        u8"-style",
        u8"-sub",
        u8"-summary",
        u8"-sup",
        u8"-table",
        u8"-tbody",
        u8"-td",
        u8"-tel",
        u8"-text",
        u8"-tfoot",
        u8"-th",
        u8"-thead",
        u8"-tip",
        u8"-todo",
        u8"-tr",
        u8"-trim",
        u8"-tt",
        u8"-u",
        u8"-ul",
        u8"-unprocessed",
        u8"-url",
        u8"-var",
        u8"-warning",
        u8"-wbr",
        u8"-wg21-example",
        u8"-wg21-grammar",
        u8"-wg21-head",
        u8"-wg21-note",
        u8"-wg21_example",
        u8"-wg21_grammar",
        u8"-wg21_head",
        u8"-wg21_note",
    };
    // clang-format on
    static_assert(std::ranges::is_sorted(prefixed_names));
    static_assert(prefixed_names[0][0] == builtin_directive_prefix);

    static constexpr auto all_names = [] {
        std::array<std::u8string_view, std::size(prefixed_names) * 2> result;
        std::ranges::copy(prefixed_names, result.data());
        std::ranges::copy(
            prefixed_names
                | std::views::transform([](std::u8string_view n) { return n.substr(1); }),
            result.data() + std::size(prefixed_names)
        );
        return result;
    }();
    const Distant<std::size_t> result
        = closest_match(all_names, name, context.get_transient_memory());
    if (!result) {
        return {};
    }
    return { .value = all_names[result.value], .distance = result.distance };
}

Directive_Behavior*
Builtin_Directive_Set::operator()(std::u8string_view name, Context& context) const
{
    // Any builtin names should be found with both `\\-directive` and `\\directive`.
    // `\\def` does not permit defining directives with a hyphen prefix,
    // so this lets the user
    if (name.starts_with(builtin_directive_prefix)) {
        return (*this)(name.substr(1), context);
    }
    if (name.empty()) {
        return nullptr;
    }
    // NOLINTBEGIN(readability-braces-around-statements)
    switch (name[0]) {
    case u8'a':
        if (name == u8"abstract")
            return &m_impl->abstract;
        break;

    case u8'B':
        if (name == u8"Babstract")
            return &m_impl->Babstract;
        if (name == u8"Bdecision")
            return &m_impl->Bdecision;
        if (name == u8"Bdel")
            return &m_impl->Bdel;
        if (name == u8"Bdetails")
            return &m_impl->Bdetails;
        if (name == u8"Bdiff")
            return &m_impl->Bdiff;
        if (name == u8"Bex")
            return &m_impl->Bex;
        if (name == u8"Bimp")
            return &m_impl->Bimp;
        if (name == u8"Bindent")
            return &m_impl->Bindent;
        if (name == u8"Bins")
            return &m_impl->Bins;
        if (name == u8"Bnote")
            return &m_impl->Bnote;
        if (name == u8"Bquote")
            return &m_impl->Bquote;
        if (name == u8"Btip")
            return &m_impl->Btip;
        if (name == u8"Btodo")
            return &m_impl->Btodo;
        if (name == u8"Bug")
            return &m_impl->Bug;
        if (name == u8"Bwarn")
            return &m_impl->Bwarn;
        break;

    case u8'b':
        if (name == u8"b")
            return &m_impl->b;
        if (name == u8"bib")
            return &m_impl->bib;
        if (name == u8"block")
            return &m_impl->block;
        if (name == u8"blockquote")
            return &m_impl->blockquote;
        if (name == u8"br")
            return &m_impl->br;
        if (name == u8"bug")
            return &m_impl->bug;
        break;

    case u8'C':
        if (name == u8"Cadd")
            return &m_impl->Cadd;
        if (name == u8"Cdiv")
            return &m_impl->Cdiv;
        if (name == u8"Cmul")
            return &m_impl->Cmul;
        if (name == u8"Csub")
            return &m_impl->Csub;
        break;

    case u8'c':
        if (name == u8"c")
            return &m_impl->c;
        if (name == u8"caption")
            return &m_impl->caption;
        if (name == u8"cite")
            return &m_impl->cite;
        if (name == u8"code")
            return &m_impl->code;
        if (name == u8"codeblock")
            return &m_impl->codeblock;
        if (name == u8"comment")
            return &m_impl->comment;
        break;

    case u8'd':
        if (name == u8"dd")
            return &m_impl->dd;
        if (name == u8"decision")
            return &m_impl->decision;
        if (name == u8"del")
            return &m_impl->del;
        if (name == u8"delblock")
            return &m_impl->delblock;
        if (name == u8"details")
            return &m_impl->details;
        if (name == u8"dfn")
            return &m_impl->dfn;
        if (name == u8"diff")
            return &m_impl->diff;
        if (name == u8"div")
            return &m_impl->div;
        if (name == u8"dl")
            return &m_impl->dl;
        if (name == u8"dt")
            return &m_impl->dt;
        break;

    case u8'e':
        if (name == u8"em")
            return &m_impl->em;
        if (name == u8"error")
            return &m_impl->error;
        if (name == u8"example")
            return &m_impl->example;
        break;

    case u8'g':
        if (name == u8"gterm")
            return &m_impl->gterm;
        break;

    case u8'h':
        if (name == u8"h1")
            return &m_impl->h1;
        if (name == u8"h2")
            return &m_impl->h2;
        if (name == u8"h3")
            return &m_impl->h3;
        if (name == u8"h4")
            return &m_impl->h4;
        if (name == u8"h5")
            return &m_impl->h5;
        if (name == u8"h6")
            return &m_impl->h6;
        if (name == u8"here")
            return &m_impl->here;
        if (name == u8"hereblock")
            return &m_impl->hereblock;
        if (name == u8"hl")
            return &m_impl->hl;
        if (name == u8"hr")
            return &m_impl->hr;
        if (name == u8"html")
            return &m_impl->html;
        if (name == u8"htmlblock")
            return &m_impl->htmlblock;
        static_assert(html_tag_prefix[0] == 'h');
        if (name.starts_with(html_tag_prefix))
            return &m_impl->html_tags;
        break;

    case u8'i':
        if (name == u8"i")
            return &m_impl->i;
        if (name == u8"import")
            return &m_impl->import;
        if (name == u8"important")
            return &m_impl->important;
        if (name == u8"include")
            return &m_impl->include;
        if (name == u8"indent")
            return &m_impl->indent;
        if (name == u8"inline")
            return &m_impl->in_line;
        if (name == u8"ins")
            return &m_impl->ins;
        if (name == u8"insblock")
            return &m_impl->insblock;
        break;

    case u8'k':
        if (name == u8"kbd")
            return &m_impl->kbd;
        break;

    case u8'l':
        if (name == u8"li")
            return &m_impl->li;
        if (name == u8"literally")
            return &m_impl->literally;
        if (name == u8"lorem-ipsum")
            return &m_impl->hyphen_lorem_ipsum;
        if (name == u8"lorem_ipsum")
            return &m_impl->lorem_ipsum;
        break;

    case u8'N':
        if (name == u8"N")
            return &m_impl->N;
        break;

    case u8'm':
        if (name == u8"macro")
            return &m_impl->macro;
        if (name == u8"mail")
            return &m_impl->mail;
        if (name == u8"make-bib")
            return &m_impl->hyphen_make_bib;
        if (name == u8"make-contents")
            return &m_impl->hyphen_make_contents;
        if (name == u8"make_bib")
            return &m_impl->make_bib;
        if (name == u8"make_contents")
            return &m_impl->make_contents;
        if (name == u8"mark")
            return &m_impl->mark;
        if (name == u8"math")
            return &m_impl->math;
        if (name == u8"mathblock")
            return &m_impl->mathblock;
        break;

    case u8'n':
        if (name == u8"nobr")
            return &m_impl->nobr;
        if (name == u8"noscript")
            return &m_impl->noscript;
        if (name == u8"note")
            return &m_impl->note;
        break;

    case u8'o':
        if (name == u8"o")
            return &m_impl->o;
        if (name == u8"ol")
            return &m_impl->ol;
        break;

    case u8'p':
        if (name == u8"p")
            return &m_impl->p;
        if (name == u8"paragraphs")
            return &m_impl->paragraphs;
        if (name == u8"pre")
            return &m_impl->pre;
        break;

    case u8'q':
        if (name == u8"q")
            return &m_impl->q;
        break;

    case u8'r':
        if (name == u8"ref")
            return &m_impl->ref;
        break;

    case u8's':
        if (name == u8"s")
            return &m_impl->s;
        if (name == u8"samp")
            return &m_impl->samp;
        if (name == u8"sans")
            return &m_impl->sans;
        if (name == u8"script")
            return &m_impl->script;
        if (name == u8"serif")
            return &m_impl->serif;
        if (name == u8"small")
            return &m_impl->small;
        if (name == u8"span")
            return &m_impl->span;
        if (name == u8"strong")
            return &m_impl->strong;
        if (name == u8"style")
            return &m_impl->style;
        if (name == u8"sub")
            return &m_impl->sub;
        if (name == u8"summary")
            return &m_impl->summary;
        if (name == u8"sup")
            return &m_impl->sup;
        break;

    case u8't':
        if (name == u8"table")
            return &m_impl->table;
        if (name == u8"tbody")
            return &m_impl->tbody;
        if (name == u8"td")
            return &m_impl->td;
        if (name == u8"tel")
            return &m_impl->tel;
        if (name == u8"text")
            return &m_impl->text;
        if (name == u8"tfoot")
            return &m_impl->tfoot;
        if (name == u8"th")
            return &m_impl->th;
        if (name == u8"thead")
            return &m_impl->thead;
        if (name == u8"there")
            return &m_impl->there;
        if (name == u8"tip")
            return &m_impl->tip;
        if (name == u8"todo")
            return &m_impl->todo;
        if (name == u8"tr")
            return &m_impl->tr;
        if (name == u8"trim")
            return &m_impl->trim;
        if (name == u8"tt")
            return &m_impl->tt;
        break;

    case u8'U':
        if (name == u8"U")
            return &m_impl->U;
        if (name == u8"Udigits")
            return &m_impl->Udigits;
        break;

    case u8'u':
        if (name == u8"u")
            return &m_impl->u;
        if (name == u8"ul")
            return &m_impl->ul;
        if (name == u8"unprocessed")
            return &m_impl->unprocessed;
        if (name == u8"url")
            return &m_impl->url;
        break;

    case u8'V':
        if (name == u8"Vget")
            return &m_impl->Vget;
        if (name == u8"Vset")
            return &m_impl->Vset;
        break;

    case u8'v':
        if (name == u8"var")
            return &m_impl->var;
        break;

    case u8'w':
        if (name == u8"warning")
            return &m_impl->warning;
        if (name == u8"wbr")
            return &m_impl->wbr;
        if (name == u8"wg21-example")
            return &m_impl->hyphen_wg21_example;
        if (name == u8"wg21-grammar")
            return &m_impl->hyphen_wg21_grammar;
        if (name == u8"wg21-head")
            return &m_impl->hyphen_wg21_head;
        if (name == u8"wg21-note")
            return &m_impl->hyphen_wg21_note;
        if (name == u8"wg21_example")
            return &m_impl->wg21_example;
        if (name == u8"wg21_grammar")
            return &m_impl->wg21_grammar;
        if (name == u8"wg21_head")
            return &m_impl->wg21_head;
        if (name == u8"wg21_note")
            return &m_impl->wg21_note;
        if (name == u8"word")
            return &m_impl->word;
        break;

    default: break;
    }
    // NOLINTEND(readability-braces-around-statements)

    return nullptr;
}

} // namespace cowel
