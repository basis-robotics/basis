#include <memory>
#include <span>
#include <thread>

#include <gtest/gtest.h>
#include <basis/plugins/transport/tcp.h>
#include "spdlog/cfg/env.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h"

// removeme
#include <queue>
#include <sys/epoll.h>
#include <fcntl.h>
#include <basis/core/threading/thread_pool.h>


using namespace basis::core::networking;

namespace basis::plugins::transport {

struct Epoll {
    // https://stackoverflow.com/questions/39173429/one-shot-level-triggered-epoll-does-epolloneshot-imply-epollet
    // https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/

    // we don't want to be using edge triggered mode per thread
    // it will force us to drain the socket completely
    // if this happens enough times we will run out of available workers
    
    // instead, level triggered once, but with a work queue
    // one thread pool
    // one main thread, using epoll_wait
    // when epoll_wait is ready add an event to the queue, can call a condition variable once
    //    ...we can probably actually add _all_ events to the queue and notify_all
    // when a thread wakes up due to cv, it pulls an event off the queue
    // when a thread gets EAGAIN on reading, it rearms the fd
    // when a thread finishes a read without EAGAIN, it puts the work back on the queue to signal that there's more to be done
    // when the thread loops, it locks and checks if there's work to be done, no need to wait for cv

    // todo: check for CAP_BLOCK_SUSPEND
    Epoll() {
    	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        assert(epoll_fd != -1);

        epoll_main_thread = std::thread(&Epoll::MainThread, this);
    }

    /*
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
    Epoll(Epoll&& other) = default;
    Epoll& operator=(Epoll&& other) = default;
*/
    ~Epoll() {
        spdlog::debug("~Epoll");
        stop = true;
        if(epoll_main_thread.joinable()) {
            epoll_main_thread.join();
        }
        close(epoll_fd);

    }

    void MainThread() {
        // https://linux.die.net/man/4/epoll
        constexpr int MAX_EVENTS = 128;
        epoll_event events[128];
        while(!stop) {
            // Wait for events, indefinitely - rely on close() to stop us
            // todo: we could send a signal or add an additional socket for epoll to listen to
            // to get faster shutdown
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
            if(nfds < 0) {
                spdlog::debug("epoll dieing");
                return;
            }
            for(int n = 0; n < nfds; ++n) {
                int fd = events[n].data.fd;
                spdlog::trace("Socket {} ready.", events[n].data.fd);
                callbacks.at(fd)(fd);
            }
        }
    }

    /**
     * Adds the fd to the watch interface.
     * Forces the fd to be non-blocking.
     *
     * @todo would it be useful to allow blocking sockets here?
     * @todo should we enforce a Socket& instead?
     * @todo need to ensure that we remove the handle from epoll before we close it. This means a two way reference
     * @todo a careful reading of the epoll spec implies that one should read from the socket once after adding here
     * @todo error handling
     * @todo return a handle that can reactivate itself 
     */
    bool AddFd(int fd, std::function<void(int)> callback) {
        assert(callbacks.count(fd) == 0);
        spdlog::debug("AddFd {}" , fd);
        int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // EPOLLEXCLUSIVE shouldn't be needed for single threaded epoll
        event.data.fd = fd;
        
        // This is safe across threads
        // https://bugzilla.kernel.org/show_bug.cgi?id=43072
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
            spdlog::debug("Failed to add file descriptor to epoll");
            return false;
        }
        callbacks[fd] = callback;
        return true;
    }

    bool ReactivateHandle(int fd) {
        epoll_event event;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // EPOLLEXCLUSIVE shouldn't be needed for single threaded epoll
        event.data.fd = fd;
        
        // This is safe across threads
        // https://bugzilla.kernel.org/show_bug.cgi?id=43072
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event)) {
            spdlog::debug("Failed to wake up fd with epoll");
            return false;
        }
        return true;
    }
    // todo https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/
    std::thread epoll_main_thread;
    int epoll_fd = -1;
    std::atomic<bool> stop = false;
    // do we really need multiple callbacks or can we just use one for the whole thing?
    std::unordered_map<int, std::function<void(int)>> callbacks;
};


class TestTcpTransport : public testing::Test {
public:
    TestTcpTransport() {
        spdlog::set_level(spdlog::level::debug);
    }

    TcpListenSocket CreateListenSocket(uint16_t port = 4242) {
        spdlog::debug("Create TcpListenSocket");
        auto maybe_listen_socket = TcpListenSocket::Create(4242);
        EXPECT_TRUE(maybe_listen_socket.has_value());
        return {std::move(maybe_listen_socket.value())};
    }

    std::unique_ptr<TcpReceiver> SubscribeToPort(uint16_t port = 4242) {
        spdlog::debug("Construct TcpReceiver");
        auto receiver = std::make_unique<TcpReceiver>("127.0.0.1", 4242);
        EXPECT_FALSE(receiver->IsConnected());
        receiver->Connect();

        spdlog::debug("Connect TcpReceiver to listen socket");
        EXPECT_TRUE(receiver->IsConnected());
        return receiver;
    }

    std::unique_ptr<TcpSender> AcceptOneKnownClient(TcpListenSocket& listen_socket) {
        spdlog::debug("Check for new connections via Accept");
        auto maybe_sender_socket = listen_socket.Accept(-1);
        EXPECT_TRUE(maybe_sender_socket.has_value());
        auto sender = std::make_unique<TcpSender>(std::move(maybe_sender_socket.value()));
        EXPECT_TRUE(sender->IsConnected());
        return std::move(sender);
    }

    // Work around privateness
    bool Send(TcpSender& sender, const std::byte* data, size_t len) {
        return sender.Send(data, len);
    }

};

TEST_F(TestTcpTransport, NoCoordinator) {
    TcpListenSocket listen_socket = CreateListenSocket();    
    std::unique_ptr<TcpReceiver> receiver = SubscribeToPort();
    std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

    spdlog::debug("Send raw bytes");
    const std::string hello = "Hello, World!";
    Send(*sender, (std::byte*)hello.c_str(), hello.size() + 1);

    spdlog::debug("Receive raw bytes");
    char buffer[1024];
    receiver->Receive((std::byte*)buffer, hello.size() + 1, 1);
    ASSERT_STREQ(buffer, hello.c_str());
    memset(buffer, 0, sizeof(buffer));

    spdlog::debug("Construct and send proper message packet");
    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), hello.data());
    sender->SendMessage(shared_message);

    spdlog::debug("Sleep for a bit to allow the message to send");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    spdlog::debug("Done sleeping");
    
    spdlog::debug("Get the message");
    auto msg = receiver->ReceiveMessage(1.0);
    ASSERT_NE(msg, nullptr);

    spdlog::debug("Validate the header");
    const core::transport::MessageHeader* header = msg->GetMessageHeader();
    spdlog::debug("Magic is {}{}", std::string_view((char*)header->magic_version, 3), header->magic_version[3]);
    ASSERT_EQ(memcmp(header->magic_version, std::array<char, 4>{'B', 'A', 'S', 0}.data(), 4), 0);
    ASSERT_EQ(header->data_size, hello.size() + 1);
    ASSERT_STREQ((const char*)msg->GetPayload().data(), hello.c_str());

    sender->SendMessage(shared_message);
    sender->SendMessage(shared_message);
    sender->SendMessage(shared_message);
    
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    // This should fail, we've run out of messages
    ASSERT_EQ(receiver->ReceiveMessage(1.0), nullptr);
}

TEST_F(TestTcpTransport, WorkQueue) {
    TcpListenSocket listen_socket = CreateListenSocket();    
    std::unique_ptr<TcpReceiver> receiver = SubscribeToPort();
    std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

    const std::string hello = "Hello, World!";

    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), hello.data());
    Epoll poller;

    core::transport::IncompleteRawMessage incomplete;
    auto callback = [&incomplete, &receiver, &poller, &hello](int fd) {
        spdlog::info("Running poller callback");
        
        if(receiver->ReceiveMessage(incomplete)) {
            spdlog::info("Got full message");
            auto msg = incomplete.GetCompletedMessage();
            ASSERT_NE(msg, nullptr);
            spdlog::info("{}",  spdlog::to_hex(msg->GetPacket()));
            spdlog::info("{}", (const char*)msg->GetPayload().data());
            ASSERT_STREQ((const char*)msg->GetPayload().data(), hello.c_str());
        }
        else {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                spdlog::info("Got error {}", strerror(errno));
            }
        }
        poller.ReactivateHandle(receiver->GetSocket().GetFd());
    };

    ASSERT_TRUE(poller.AddFd(receiver->GetSocket().GetFd(), callback));

    std::span<const std::byte> packet = shared_message->GetPacket();
    for(int i = 0; i < packet.size(); i++) {
        // todo: learn how to iterate on spans
        spdlog::trace("Sending byte {}: {}", i, *(packet.data() + i));
        Send(*sender, packet.data() + i, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

            
    sender->SendMessage(shared_message);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // todo: test error conditions

    // 0. TCP socket is configured as non-blocking
    // 1. Worker pool gets notification that there's a waiting socket for a receiver
    // 2. It runs recv and tries to fill output until there's no more data
    // 3. 
}


TEST_F(TestTcpTransport, Poller) {
    TcpListenSocket listen_socket = CreateListenSocket();    
    std::unique_ptr<TcpReceiver> receiver = SubscribeToPort();
    std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

    const std::string hello = "Hello, World!";

    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), hello.data());

    Epoll poller;
    core::threading::ThreadPool thread_pool(4);

    core::transport::IncompleteRawMessage incomplete;
    auto callback = [&](int fd) {
        thread_pool.enqueue([&] {
            spdlog::info("Running poller callback");
            
            if(receiver->ReceiveMessage(incomplete)) {
                spdlog::info("Got full message");
                auto msg = incomplete.GetCompletedMessage();
                ASSERT_NE(msg, nullptr);
                spdlog::info("{}",  spdlog::to_hex(msg->GetPacket()));
                spdlog::info("{}", (const char*)msg->GetPayload().data());
                ASSERT_STREQ((const char*)msg->GetPayload().data(), hello.c_str());
            }
            else {
                if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    spdlog::info("Got error {}", strerror(errno));
                }
            }
            poller.ReactivateHandle(receiver->GetSocket().GetFd());
        });
    };

    ASSERT_TRUE(poller.AddFd(receiver->GetSocket().GetFd(), callback));

}

}
