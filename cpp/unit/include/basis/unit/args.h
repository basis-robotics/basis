#include <argparse/argparse.hpp>

#include <string>

#include "args_command_line.h"

namespace basis::unit {

struct ArgumentMetadataBase {
    ArgumentMetadataBase(const std::string& name,
        const std::string& help_text,
        const std::string& type_name,
        bool optional) : name(name), help_text(help_text), type_name(type_name), optional(optional) {

        }
    virtual ~ArgumentMetadataBase() = default;

    virtual void CreateArgparseArgument(argparse::ArgumentParser& parser) = 0;

    std::string name;
    std::string help_text;
    std::string type_name;
    bool optional = false;
};

template<typename T, bool IS_OPTIONAL>
struct ArgumentOptionalHelper
{
    using type = T;
};

template<typename T>
struct ArgumentOptionalHelper<T, true>
{
    using type = std::optional<T>;
};

template<typename T_ARGUMENT_TYPE>
struct ArgumentMetadata : public ArgumentMetadataBase {
    ArgumentMetadata(const std::string& name,
        const std::string& help_text,
        const std::string& type_name,
        bool optional,
        std::optional<T_ARGUMENT_TYPE> default_value = {})
            : ArgumentMetadataBase(name, help_text, type_name, optional),
            default_value(default_value) {
    }

    virtual void CreateArgparseArgument(argparse::ArgumentParser& parser) override {
        auto& arg = parser.add_argument("--" + ArgumentMetadataBase::name)
            .help(ArgumentMetadataBase::help_text);

        if(!ArgumentMetadataBase::optional) {
            arg.required();
        }

        if constexpr (std::is_floating_point_v<T_ARGUMENT_TYPE>) {
            arg.template scan<'g', T_ARGUMENT_TYPE>();
        } else if constexpr (std::is_arithmetic_v<T_ARGUMENT_TYPE>) {
            arg.template scan<'i', T_ARGUMENT_TYPE>();
        }
        if(default_value) {
            arg.default_value(*default_value);
        }
        
    }

    std::optional<T_ARGUMENT_TYPE> default_value;
};


template<typename T_DERIVED>
struct UnitArguments {
    static std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser() {
        auto parser = std::make_unique<argparse::ArgumentParser>();

        for(auto& arg : T_DERIVED::argument_metadatas) {
            arg->CreateArgparseArgument(*parser);
        }

        return parser;
    }

    static nonstd::expected<T_DERIVED, std::string> ParseArgumentsVariant(const CommandLineTypes& command_line) {
        return std::visit([](auto&& command_line) { 
            return ParseArguments(command_line);
        }, command_line);
    }

    static nonstd::expected<T_DERIVED, std::string>  ParseArguments(const std::vector<std::pair<std::string, std::string>>& command_line) {
        std::vector<std::string> command_line_exploded(command_line.size() * 2);
        for(auto& [k, v] : command_line) {
            command_line_exploded.push_back("--" + k);
            command_line_exploded.push_back(v);
        }

        return ParseArguments(command_line_exploded);
    }

    static nonstd::expected<T_DERIVED, std::string>  ParseArguments(std::pair<int, const char*const*> argc_argv) {
        return ParseArguments(argc_argv.first, argc_argv.second);
    }

    static nonstd::expected<T_DERIVED, std::string>  ParseArguments(int argc, const char*const* argv) {
        // https://github.com/p-ranav/argparse/blob/v3.1/include/argparse/argparse.hpp#L1868
        return ParseArguments(std::vector<std::string>{argv, argv + argc});
    }

    static nonstd::expected<T_DERIVED, std::string>  ParseArguments(const std::vector<std::string>& command_line) {
        T_DERIVED out;

        auto parser = CreateArgumentParser();
        
        try {
            parser->parse_args(command_line);
        }
        catch (const std::exception& err) {
            return nonstd::make_unexpected(err.what());
        }

        // TODO: this can likely be done via metadata
        out.HandleParsedArgs(*parser);

        return out;
    }
};

}