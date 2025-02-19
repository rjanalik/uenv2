#include <CLI/CLI.hpp>
#include <fmt/core.h>

#include <util/completion.h>

namespace util {

namespace completion {

// void normalize_bash_function_name(std::string &str) {
//     std::replace(str.begin(), str.end(), '-', '_');
// }

// std::vector<CLI::Option *> filter(std::vector<CLI::Option *> in, bool (*f)(CLI::Option *)) {
//     std::vector<const CLI::Option*> out;
//     out.reserve(in.size());
//     std::copy_if(in.begin(), in.end(),
//                  std::back_inserter(out), f);
//     return out;
// }

template<typename Ts, typename Td>
std::vector<Td> map(std::vector<Ts> in, Td (*f)(Ts)) {
    std::vector<std::string> out;
    out.reserve(in.size());
    std::transform(in.begin(), in.end(),
                   std::back_inserter(out),
                   f);
    return out;
}

template<typename Ts, typename Td>
Td reduce(std::vector<Ts> in, Td (*f)(Td, Ts), Td init) {
    return std::accumulate(in.begin(), in.end(), init, f);
}

template<typename T>
T reduce(std::vector<T> in, T (*f)(T, T), T init) {
    return reduce<T, T>(in, f, init);
}

template<typename T, typename F>
bool is_in(std::vector<T> vec, F&& f) {
    return std::any_of(vec.begin(),
                       vec.end(), f);
}

std::string get_prefix(CLI::App* cli) {
    if (cli == nullptr)
        return "";

    CLI::App* parent = cli->get_parent();
    return get_prefix(parent) + "_" + cli->get_name();
}

void create_completion_rec(CLI::App* cli) {
    std::string func_name = get_prefix(cli);
    //auto options = cli->get_options();
    auto subcommands = cli->get_subcommands({});

    auto is_positional = [](CLI::Option* option) {
        return option->get_positional();
    };
    auto is_non_positional = [](CLI::Option* option) {
        return option->nonpositional();
    };
    std::vector<CLI::Option*> options_positional = cli->get_options(is_positional);
    std::vector<CLI::Option*> options_non_positional = cli->get_options(is_non_positional);

    auto get_option_name = [](CLI::Option* option) {
        return option->get_name();
    };
    // auto get_subcommand_name = [](CLI::App* subcommand) {
    //     return subcommand->get_name();
    // };

    auto concatenate_option_names = [](std::string str, CLI::Option* option) {
        return str + " " + option->get_name();
    };
    auto concatenate_subcommand_names = [](std::string str, CLI::App* subcommand) {
        return str + " " + subcommand->get_name();
    };

    // TODO generic for all special args
    // TODO optimize the functions (reuse if possible)
    auto has_uenv = [get_option_name](CLI::Option* option) {
        return get_option_name(option) == "uenv";
    };
    bool special_uenv = is_in<CLI::Option*>(options_positional, has_uenv);

    std::string completions = reduce<CLI::Option*, std::string>(options_non_positional, concatenate_option_names, "")
                            + reduce<CLI::App*, std::string>(subcommands, concatenate_subcommand_names, "");

    fmt::print(R"({func_name}()
{{
    UENV_OPTS="{completions}"
)",
               fmt::arg("func_name", func_name),
               fmt::arg("completions", completions));
    // TODO generic for all special args
    if (special_uenv) {
        std::string special_func_name = "_uenv_special_uenv";
        std::string special_opts_name = "UENV_SPECIAL_OPTS_UENV";

        fmt::print(R"(
    if typeset -f {special_func_name} >/dev/null
    then
        {special_func_name}
        UENV_OPTS+=" ${{{special_opts_name}}}"
    fi
)",
                   fmt::arg("special_func_name", special_func_name),
                   fmt::arg("special_opts_name", special_opts_name));
    }
    fmt::print("}}\n\n");

    for (auto* subcom : subcommands)
        create_completion_rec(subcom);
}

void create_completion(CLI::App* cli) {
    create_completion_rec(cli);

    fmt::print(R"(
_uenv_special_uenv()
{{
    UENV_SPECIAL_OPTS_UENV=$(uenv image ls --no-header | awk '{{print $1}}')
}}

_uenv_completions()
{{
    local cur prefix func_name UENV_OPTS

    local -a COMP_WORDS_NO_FLAGS
    local index=0
    while [[ "$index" -lt "$COMP_CWORD" ]]
    do
        if [[ "${{COMP_WORDS[$index]}}" == [a-z]* ]]
        then
            COMP_WORDS_NO_FLAGS+=("${{COMP_WORDS[$index]}}")
        fi
        let index++
    done
    COMP_WORDS_NO_FLAGS+=("${{COMP_WORDS[$COMP_CWORD]}}")
    local COMP_CWORD_NO_FLAGS=$((${{#COMP_WORDS_NO_FLAGS[@]}} - 1))

    cur="${{COMP_WORDS_NO_FLAGS[COMP_CWORD_NO_FLAGS]}}"
    prefix="_${{COMP_WORDS_NO_FLAGS[*]:0:COMP_CWORD_NO_FLAGS}}"
    func_name="${{prefix// /_}}"
    func_name="${{func_name//-/_}}"

    UENV_OPTS=""
    if typeset -f $func_name >/dev/null
    then
        $func_name
    fi

    COMPREPLY=($(compgen -W "${{UENV_OPTS}}" -- "${{cur}}"))
}}

complete -F _uenv_completions uenv
)");
}

} // namespace completion
} // namespace util
