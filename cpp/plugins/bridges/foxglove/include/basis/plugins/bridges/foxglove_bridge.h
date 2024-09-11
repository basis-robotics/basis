#pragma once

#include <basis/unit.h>

#include <foxglove/websocket/websocket_server.hpp>

namespace basis::plugins::bridges::foxglove {
class FoxgloveBridge : public basis::SingleThreadedUnit {
  using ConnectionHandle = websocketpp::connection_hdl;
  using ClientPublications =
      std::unordered_map<::foxglove::ClientChannelId, std::shared_ptr<basis::core::transport::PublisherRaw>>;
  using PublicationsByClient = std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
  using SubscriptionsByClient =
      std::map<ConnectionHandle, std::shared_ptr<basis::core::transport::SubscriberBase>, std::owner_less<>>;

public:
  FoxgloveBridge(std::string unit_name);

  ~FoxgloveBridge();

  void Initialize(const UnitInitializeOptions &options) override;

  void Update(const basis::core::Duration &max_sleep_duration) override;

private:
  struct TopicAndDatatype {
    std::string topic;
    std::string schema_serializer;
    std::string schema;
  };

  void init(const std::string &address = "0.0.0.0", int port = 8765);

  void logHandler(::foxglove::WebSocketLogLevel level, [[maybe_unused]] char const *msg);

  void subscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle);

  void unsubscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle);

  void clientAdvertise(const ::foxglove::ClientAdvertisement &channel, ConnectionHandle clientHandle);

  void clientUnadvertise(::foxglove::ClientChannelId channelId, ConnectionHandle clientHandle);

  void clientMessage(const ::foxglove::ClientMessage &clientMsg, ConnectionHandle clientHandle);

  void updateAdvertisedTopics();

  std::unique_ptr<::foxglove::ServerInterface<ConnectionHandle>> server;
  bool useSimTime = false;

  std::mutex subscriptionsMutex;
  std::shared_mutex publicationsMutex;

  std::unordered_map<::foxglove::ChannelId, ::foxglove::ChannelWithoutId> advertisedTopics;
  std::unordered_map<::foxglove::ChannelId, SubscriptionsByClient> subscriptions;

  PublicationsByClient clientAdvertisedTopics;

  std::vector<std::regex> topicWhitelistPatterns = {std::regex(".*")};
};

} // namespace basis::plugins::bridges::foxglove