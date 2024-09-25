#include <argparse/argparse.hpp>

#include <string>

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
    std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser() {
        auto parser = std::make_unique<argparse::ArgumentParser>();

        for(auto& arg : T_DERIVED::argument_metadatas) {
            arg->CreateArgparseArgument(*parser);
        }

        return parser;
    }

    T_DERIVED ParseArguments(const std::map<std::string, std::string>& args) {
        std::vector<std::string> command_line(args.size() * 2);
        for(auto& [k, v] : args) {
            command_line.push_back("--" + k);
            command_line.push_back(v);
        }

        return ParseArguments(command_line);
    }
    T_DERIVED ParseArguments(int argc, const char* argv[]) {
        // https://github.com/p-ranav/argparse/blob/v3.1/include/argparse/argparse.hpp#L1868
        return ParseArguments(std::vector<std::string>{argv, argv + argc});
    }

    T_DERIVED ParseArguments(const std::vector<std::string>& command_line) {
        T_DERIVED out;

        auto parser = CreateArgumentParser();
        
        parser->parse_args(command_line);

        out.HandleParsedArgs(parser);
        // TODO: this can be doing via metadata

        // TODO
        return out;
    }
};

}