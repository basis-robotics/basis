#include <memory>
#include <vector>

#include <basis/unit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <simple_pub_sub.pb.h>

#pragma clang diagnostic pop


#include "handlers/SimpleSub.h"


namespace unit::simple_sub {
    // TODO: this won't work with multi threaded units, obviously
    class Base : public basis::SingleThreadedUnit {
    
        void CreatePublishersSubscribers() {
            
            // oops todo
            SimpleSub_pubsub.SetupPubSub(transport_manager.get(), &output_queue, &thread_pool);
            
        }

    public:

        virtual SimpleSub::Output SimpleSub(const SimpleSub::Input& input) = 0;


        virtual void Initialize() final override {
            CreatePublishersSubscribers();
        }
    
    private:
        
        SimpleSub::PubSub SimpleSub_pubsub  {
            [this](auto input){
                return SimpleSub(input); 
            }
        };        
    };
    
}