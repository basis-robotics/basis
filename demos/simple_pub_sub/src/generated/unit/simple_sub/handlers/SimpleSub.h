#pragma once

#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>
#include <basis/synchronizers/all.h>
#include <basis/synchronizers/field.h>


#include <basis/plugins/serialization/protobuf.h>


namespace unit::simple_sub::SimpleSub {
    struct Input {

        // /in_message
        std::shared_ptr<const StringMessage> in_message;

        // TODO: time?
        // TODO: this will need metadata around topics to handle deterministic mode
    };

    struct Output {

        // TODO: diagnostics, error state, etc
        // TODO: should we take as unique_ptr instead?
    };


    // Simple, synchronize all messages on presence
    using Synchronizer = basis::synchronizers::All<
            std::shared_ptr<const StringMessage>
    >;



    struct PubSub {
        using Callback = std::function<Output(const Input&)>;
        PubSub(

            std::function<Output(const Input&)> callback, 

            std::function<void(Output&)> publish_callback = nullptr) {
            if(!publish_callback) {
                publish_callback = 
                    [this](Output& output){
                        OnOutput(output);
                    };
            }

            synchronizer = std::make_unique<Synchronizer>(
        [this, callback, publish_callback](auto ...msgs) {
                Output output = callback({msgs...});
                publish_callback(output);
            },
            basis::synchronizers::MessageMetadata<std::shared_ptr<const StringMessage>>{
                .is_optional = false,
                .is_cached = false,
            }
            );

        }
        void SetupPubSub(
            basis::core::transport::TransportManager* transport_manager,
            basis::core::transport::OutputQueue* output_queue,
            basis::core::threading::ThreadPool* thread_pool) {
                // todo init sync
        
            in_message_subscriber = transport_manager->Subscribe<StringMessage>("/in_message", 
            [this](auto msg){
                synchronizer->OnMessage<0>(msg);
            },
            thread_pool, output_queue);
        
        
        }

        void OnOutput(const Output& output) {

        }

        // /in_message
        std::shared_ptr<basis::core::transport::Subscriber<StringMessage>> in_message_subscriber;



        std::unique_ptr<Synchronizer> synchronizer;

    };

}