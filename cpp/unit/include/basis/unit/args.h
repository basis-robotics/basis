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

        // If we aren't optional, we're required
        if(!ArgumentMetadataBase::optional) {
            arg.required();
        }

        if constexpr (std::is_same_v<T_ARGUMENT_TYPE, bool>) {
            arg.action([&](const std::string& value) { 
                std::string lowered = value;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                if(lowered == "true" or lowered == "1") {
                    return true;
                }
                if(lowered == "false" or lowered == "0") {
                    return false;
                }
                throw std::runtime_error(fmt::format("[--{} {}] can't be converted to bool, must be '0', '1', 'true', or 'false' (case insensitive)", ArgumentMetadataBase::name, value));
            });
        }
        else if constexpr (std::is_floating_point_v<T_ARGUMENT_TYPE>) {
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

    static nonstd::expected<T_DERIVED, std::string>  ParseArguments(const std::vector<std::pair<std::string, std::string>>& argument_pairs) {
        std::vector<std::string> command_line;

        command_line.push_back("");
        command_line.reserve(command_line.size());
        
        for(auto& [k, v] : argument_pairs) {
            command_line.push_back("--" + k);
            command_line.push_back(v);
        }

        return ParseArgumentsInternal(command_line);
    }

    static nonstd::expected<T_DERIVED, std::string> ParseArguments(std::pair<int, const char*const*> argc_argv) {
        // In this case, it's assumed you have a raw command line from main()
        return ParseArguments(argc_argv.first, argc_argv.second);
    }

    static nonstd::expected<T_DERIVED, std::string> ParseArguments(int argc, const char*const* argv) {
        // https://github.com/p-ranav/argparse/blob/v3.1/include/argparse/argparse.hpp#L1868
        return ParseArgumentsInternal(std::vector<std::string>{argv, argv + argc});
    }

    static nonstd::expected<T_DERIVED, std::string> ParseArguments(const std::vector<std::string>& command_line) {
        std::vector<std::string> command_line_with_program;
        command_line_with_program.reserve(command_line.size() + 1);
        // Handle "program name" argument
        command_line_with_program.push_back("");
        command_line_with_program.insert(command_line_with_program.end(), command_line.begin(), command_line.end());
        return ParseArgumentsInternal(command_line_with_program);
    }

private:
    static nonstd::expected<T_DERIVED, std::string> ParseArgumentsInternal(const std::vector<std::string>& command_line) {
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