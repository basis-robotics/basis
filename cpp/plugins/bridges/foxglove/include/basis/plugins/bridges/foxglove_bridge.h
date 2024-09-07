#pragma once

#include <basis/unit.h>

#include <foxglove/websocket/common.hpp>
#include <foxglove/websocket/regex_utils.hpp>
#include <foxglove/websocket/server_factory.hpp>
#include <foxglove/websocket/server_interface.hpp>
#include <foxglove/websocket/websocket_server.hpp>

namespace basis::plugins::bridges::foxglove {
class FoxgloveBridge : public basis::SingleThreadedUnit {
  using ConnectionHandle = websocketpp::connection_hdl;
  using TopicAndDatatype = std::pair<std::string, std::string>;
  using ClientPublications =
      std::unordered_map<::foxglove::ClientChannelId, std::shared_ptr<basis::core::transport::PublisherRaw>>;
  using PublicationsByClient = std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
  using SubscriptionsByClient =
      std::map<ConnectionHandle, std::shared_ptr<basis::core::transport::SubscriberBase>, std::owner_less<>>;

public:
  void init(const std::string &address = "0.0.0.0", int port = 8765);
  ~FoxgloveBridge();

private:
  struct PairHash {
    template <class T1, class T2> std::size_t operator()(const std::pair<T1, T2> &pair) const {
      return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
  };

  void logHandler(::foxglove::WebSocketLogLevel level, [[maybe_unused]] char const *msg);

  void subscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle);

  void unsubscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle);

  void clientAdvertise(const ::foxglove::ClientAdvertisement &channel, ConnectionHandle clientHandle);

  void clientUnadvertise(::foxglove::ClientChannelId channelId, ConnectionHandle clientHandle);

  void clientMessage(const ::foxglove::ClientMessage &clientMsg, ConnectionHandle clientHandle);

  void updateAdvertisedTopics();

  std::unique_ptr<::foxglove::ServerInterface<ConnectionHandle>> server;
  bool useSimTime = false;

  std::mutex _subscriptionsMutex;
  std::shared_mutex _publicationsMutex;

  std::unordered_map<::foxglove::ChannelId, ::foxglove::ChannelWithoutId> _advertisedTopics;
  std::unordered_map<::foxglove::ChannelId, SubscriptionsByClient> _subscriptions;

  PublicationsByClient _clientAdvertisedTopics;

  std::vector<std::regex> _topicWhitelistPatterns;
};

} // namespace basis::plugins::bridges::foxglove