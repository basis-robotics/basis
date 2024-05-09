#include <memory>
#include <span>
#include <thread>

#include <gtest/gtest.h>
#include <basis/plugins/transport/tcp.h>


// removeme
#include <queue>
#include <sys/epoll.h>
#include <fcntl.h>



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
        printf("~Epoll\n");
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
                printf("epoll dieing\n");
                return;
            }
            for(int n = 0; n < nfds; ++n) {
                int fd = events[n].data.fd;
                callbacks.at(fd)(fd);
                printf("Socket %i ready.\n", events[n].data.fd);
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
     */
    bool AddHandle(int fd, std::function<void(int)> callback) {
        assert(callbacks.count(fd) == 0);
        printf("AddHandle %i\n" , fd);
        int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // EPOLLEXCLUSIVE shouldn't be needed for single threaded epoll
        event.data.fd = fd;
        
        // This is safe across threads
        // https://bugzilla.kernel.org/show_bug.cgi?id=43072
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
            printf("Failed to add file descriptor to epoll\n");
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
            printf("Failed to wake up fd with epoll\n");
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


TEST(TcpTransport, NoCoordinator) {
    auto maybe_listen_socket = TcpListenSocket::Create(4242);
    ASSERT_TRUE(maybe_listen_socket.has_value());
    TcpListenSocket socket{std::move(maybe_listen_socket.value())};
    
    printf("Make recv\n");
    auto receiver = std::make_unique<TcpReceiver>("127.0.0.1", 4242);
    ASSERT_FALSE(receiver->IsConnected());
    receiver->Connect();
    ASSERT_TRUE(receiver->IsConnected());

    printf("accept\n");
    auto maybe_sender_socket = socket.Accept(1);
    ASSERT_TRUE(maybe_sender_socket.has_value());
    TcpSender sender(std::move(maybe_sender_socket.value()));
    ASSERT_TRUE(sender.IsConnected());

    printf("send\n");
    const std::string message = "Hello, World!";
    sender.Send((std::byte*)message.c_str(), message.size() + 1);
    printf("recv\n");
    char buffer[1024];
    receiver->Receive((std::byte*)buffer, message.size() + 1, 1);

    ASSERT_STREQ(buffer, message.c_str());
    printf("make shared msg\n");
    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, message.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), message.data());
    printf("Mutable string is %s\n", shared_message->GetMutablePayload().data());
    sender.SendMessage(shared_message);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    printf("Done sleeping\n");
    buffer[0] = 0;

    auto msg = receiver->ReceiveMessage(1.0);
    ASSERT_NE(msg, nullptr);

    const core::transport::MessageHeader* header = msg->GetMessageHeader();
    printf("Magic is %c %c %c %i\n", header->magic_version[0], header->magic_version[1], header->magic_version[2], header->magic_version[3]);
    ASSERT_EQ(memcmp(header->magic_version, std::array<char, 4>{'B', 'A', 'S', 0}.data(), 4), 0);

    ASSERT_STREQ((const char*)msg->GetPayload().data(), message.c_str());

    sender.SendMessage(shared_message);
    sender.SendMessage(shared_message);
    sender.SendMessage(shared_message);
    
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
    ASSERT_EQ(receiver->ReceiveMessage(1.0), nullptr);

}

TEST(TcpTransport, Poller) {
    auto maybe_listen_socket = TcpListenSocket::Create(4242);
    ASSERT_TRUE(maybe_listen_socket.has_value());
    TcpListenSocket socket{std::move(maybe_listen_socket.value())};
    
    printf("Make recv\n");
    auto receiver = std::make_unique<TcpReceiver>("127.0.0.1", 4242);
    receiver->Connect();
    ASSERT_TRUE(receiver->IsConnected());

    printf("accept\n");
    auto maybe_sender_socket = socket.Accept(1);
    ASSERT_TRUE(maybe_sender_socket.has_value());
    TcpSender sender(std::move(maybe_sender_socket.value()));
    ASSERT_TRUE(sender.IsConnected());

    const std::string message = "Hello, World!";

    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, message.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), message.data());


    // Subscribers will share a work queue - any subscribers in the same queue will be mutually exclusive
    std::queue<std::unique_ptr<basis::core::transport::RawMessage>> work_queue;


    Epoll poller;

    core::transport::IncompleteRawMessage incomplete;
    auto callback = [&incomplete, &receiver, &poller](int fd) {
        printf("callback\n");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        
        if(receiver->ReceiveMessage(incomplete)) {
            printf("Got full message\n");
            printf("%s\n", (const char*)incomplete.GetCompletedMessage()->GetPayload().data());
        }
        else {
            printf("Got error %s\n", strerror(errno));
        }
        //ASSERT_EQ(receiver->ReceiveMessage(incomplete), true);
        //ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
        //ASSERT_EQ(receiver->ReceiveMessage(1.0), nullptr);
        poller.ReactivateHandle(receiver->GetSocket().GetFd());
    };


    ASSERT_TRUE(poller.AddHandle(receiver->GetSocket().GetFd(), callback));

    // sender.SendMessage(shared_message);
    //         std::this_thread::sleep_for(std::chrono::seconds(1));

    // ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        sender.SendMessage(shared_message);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 0. TCP socket is configured as non-blocking
    // 1. Worker pool gets notification that there's a waiting socket for a receiver
    // 2. It runs recv and tries to fill output until there's no more data
    // 3. 
}

}