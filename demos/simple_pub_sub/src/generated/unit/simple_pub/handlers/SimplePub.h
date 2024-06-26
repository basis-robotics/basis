#pragma once

#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>
#include <basis/synchronizers/all.h>
#include <basis/synchronizers/field.h>


#include <basis/plugins/serialization/protobuf.h>


namespace unit::simple_pub::SimplePub {
    struct Input {

        // TODO: time?
        // TODO: this will need metadata around topics to handle deterministic mode
    };

    struct Output {

        // /chatter
        std::shared_ptr<const StringMessage> chatter;

        // TODO: diagnostics, error state, etc
        // TODO: should we take as unique_ptr instead?
    };




    struct PubSub {
        using Callback = std::function<Output(const Input&)>;
        PubSub(

            std::function<void(Output&)> publish_callback = nullptr) {
            if(!publish_callback) {
                publish_callback = 
                    [this](Output& output){
                        OnOutput(output);
                    };
            }

        }
        void SetupPubSub(
            basis::core::transport::TransportManager* transport_manager,
            basis::core::transport::OutputQueue* output_queue,
            basis::core::threading::ThreadPool* thread_pool) {
                // todo init sync
        
        
            chatter_publisher = transport_manager->Advertise<StringMessage>("/chatter");
        
        }

        void OnOutput(const Output& output) {

        chatter_publisher->Publish(output.chatter);
        }


        // /chatter
        std::shared_ptr<basis::core::transport::Publisher<StringMessage>> chatter_publisher;

    };

}