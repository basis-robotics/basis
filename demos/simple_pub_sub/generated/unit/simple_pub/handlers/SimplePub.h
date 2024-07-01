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

        // /out_message
        std::shared_ptr<const StringMessage> out_message;

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
        
        
            out_message_publisher = transport_manager->Advertise<StringMessage>("/out_message");
        
        }

        void OnOutput(const Output& output) {

        out_message_publisher->Publish(output.out_message);
        }


        // /out_message
        std::shared_ptr<basis::core::transport::Publisher<StringMessage>> out_message_publisher;

    };

}