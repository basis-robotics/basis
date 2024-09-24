#include <argparse/argparse.hpp>

#include <string>

namespace basis::unit {

struct ArgumentMetadataBase {
    std::string name;
    std::string help_text;
    std::string type_name;
    bool optional = false;
};

template<typename T_STORAGE_TYPE, typename T_ARGUMENT_TYPE>
struct ArgumentMetadata : public ArgumentMetadataBase {
    ArgumentMetadata(const std::string& name,
        const std::string& help_text,
        const std::string& type_name,
        bool optional,
        T_ARGUMENT_TYPE T_STORAGE_TYPE::* storage, std::optional<T_ARGUMENT_TYPE> default_value) : ArgumentMetadataBase(name, help_text, type_name, optional), storage(storage), default_value(default_value) {

    }

    void CreateArgparseArgument(argparse::ArgumentParser& parser) {
        auto& arg = parser.add_argument("--" + name)
            .help(help_text);
            //.scan<'i', uint16_t>()
        if(default_value) {
            arg.default_value(*default_value);
        }
    }

    T_ARGUMENT_TYPE T_STORAGE_TYPE::* storage;
    std::optional<T_ARGUMENT_TYPE> default_value;

};

}