#include <string>
#include <map>

namespace basis {
    namespace unit {
    struct Metadata {
        struct Handler {
            
        };
        // Ordered to ensure stability when iterating
        std::map<std::string, Handler> handlers;
    }
    }
}