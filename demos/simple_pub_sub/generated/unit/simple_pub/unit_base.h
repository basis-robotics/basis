#include <memory>
#include <vector>

#include <basis/unit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <simple_pub_sub.pb.h>

#pragma clang diagnostic pop


#include <unit/simple_pub/handlers/SimplePub.h>


namespace unit::simple_pub {
    // TODO: this won't work with multi threaded units, obviously
    class Base : public basis::SingleThreadedUnit {
    
        void CreatePublishersSubscribers() {
            
            // oops todo
            SimplePub_pubsub.SetupPubSub(transport_manager.get(), &output_queue, &thread_pool);
            
        }

    public:

        virtual SimplePub::Output SimplePub(const SimplePub::Input& input) = 0;


        virtual void Initialize() final override {
            CreatePublishersSubscribers();
        }
    
    private:
        
        SimplePub::PubSub SimplePub_pubsub = {
            [this](auto input){
                return SimplePub(input); 
            }
        };
        
    };
    
}