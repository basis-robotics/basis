#pragma once

#include <cstdint>

#include <basis/plugins/transport/tcp.h>

#include <transport.pb.h>

/**
 * This is a bit annoying. We have to create a custom transport for publisher info as both the coordinator socket needs to be at a well known location,
 * as well as accepting data bidirectionally, something that wasn't intended by the tcp transport design.
 * 
 * Maybe later once we have proper duplex on the tcp side we can just use a regular tcp receiver.
 * 
 * In any case, this means that the coordinator will be a completely custom stack, for now. In theory one _can_ make a coordinator using the pub/sub framework,
 * but there are a lot of chicken and egg problems here.
 * 
 * Things that will need solved:
 *   - per transport configuration (for the well known port)
 *   - duplexed communication between 
 *      - how? because of epoll we probably don't want to have two objects sharing a socket. 
 *          Likely it would mean combining sender and receiver, and letting epoll have differing callbacks for getting and sending data.
 *      - this has an additional complication - the send and recv side are different types
 *   - this is really just an RPC system
 * 
 * The short term dream here is to only use special constructions for the publisher info and spin up proper infra for every other bit of information.
 * The publish info stuff should take minimal processing and run on a single thread.
 * It will represent a small snag on the unit side - will need a helper class to assist with coordinator communication.
 * 
 * TODO: it might be simpler to just write this as a protobuf grpc server :)
 */
constexpr uint16_t BASIS_PUBLISH_INFO_PORT = 1492;

//constexpr char BASIS_PUBLISHER_INFO_TOPIC[] = "/basis/publisher_info";

namespace basis::core::transport { 

    class CoordinatorConnection : public basis::plugins::transport::TcpConnection {
    public:
        CoordinatorConnection(core::networking::TcpSocket socket) : TcpConnection(std::move(socket)) {

        }

        IncompleteMessagePacket in_progress_packet;
    };

    class Coordinator {
    public:
        std::optional<Coordinator> Create(uint16_t port = BASIS_PUBLISH_INFO_PORT) {
            // todo: maybe return the listen socket error type?
            auto maybe_listen_socket = networking::TcpListenSocket::Create(port);
            if(!maybe_listen_socket) {
                return {};
            }

            return Coordinator(std::move(maybe_listen_socket.value()));
        }
        
        Coordinator(networking::TcpListenSocket listen_socket) : listen_socket(std::move(listen_socket))
        {
            
        }

        void Update() {
            // accept new clients
            // select on all clients
            // compile new data on publishers

            //
            while(auto maybe_socket = listen_socket.Accept(0)) {
                clients.emplace_back(CoordinatorConnection(std::move(maybe_socket.value())));
            }


            for (auto it = clients.begin(); it != clients.end();) {
                auto& client = *it;
                switch (client.ReceiveMessage(client.in_progress_packet)) {
                    case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
                        auto complete = client.in_progress_packet.GetCompletedMessage();
                        spdlog::info("Got completed message");

                        // convert to proto::PublisherInfo 
                        // fallthrough
                    }
                    case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
                        // No work to be done
                        ++it;
                        break;
                    }
                    case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
                     spdlog::error("Client connection disconnect after bytes {} - got error {} {}",
                                      client.in_progress_packet.GetCurrentProgress(), errno, strerror(errno));
                        // fallthrough
                    }
                    case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
                        it = clients.erase(it);
                        break;
                    }
                }
                
            }
            
        }
        
        core::networking::TcpListenSocket listen_socket;
        
        std::vector<CoordinatorConnection> clients;
    };

    class CoordinatorConnector {
        
    };
}