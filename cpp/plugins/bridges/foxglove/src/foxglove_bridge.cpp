#include <foxglove_bridge/common.hpp>
#include <foxglove_bridge/foxglove_bridge.hpp>
#include <foxglove_bridge/server_factory.hpp>
#include <foxglove_bridge/server_interface.hpp>
#include <foxglove_bridge/websocket_server.hpp>

#include <basis/core/logging/macros.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport_manager.h>
#include <basis/plugins/transport/tcp.h>
#include <basis/plugins/transport/tcp_transport_name.h>
#include <basis/unit.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using ConnectionHandle = websocketpp::connection_hdl;
using ClientPublications =
    std::unordered_map<::foxglove::ClientChannelId, std::shared_ptr<basis::core::transport::PublisherRaw>>;
using PublicationsByClient = std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
using SubscriptionsByClient =
    std::map<ConnectionHandle, std::shared_ptr<basis::core::transport::SubscriberBase>, std::owner_less<>>;

DEFINE_AUTO_LOGGER_PLUGIN(bridges, foxglove)

DECLARE_AUTO_LOGGER_PLUGIN(bridges, foxglove)

namespace basis::plugins::bridges::foxglove {

std::vector<std::regex> parseRegexPatterns(const std::vector<std::string> &patterns) {
  std::vector<std::regex> result;
  for (const auto &pattern : patterns) {
    try {
      result.push_back(std::regex(pattern, std::regex_constants::ECMAScript | std::regex_constants::icase));
    } catch (...) {
      continue;
    }
  }
  return result;
}

class FoxgloveBridge : public basis::SingleThreadedUnit {
public:
  void init() {

    using namespace basis::core::transport;
    using namespace basis::plugins::transport;

    transport_manager = std::make_unique<TransportManager>(std::make_unique<InprocTransport>());

    transport_manager->RegisterTransport(basis::plugins::transport::TCP_TRANSPORT_NAME,
                                         std::make_unique<TcpTransport>());

    try {
      ::foxglove::ServerOptions serverOptions;
      serverOptions.capabilities =
          std::vector<std::string>(::foxglove::DEFAULT_CAPABILITIES.begin(), ::foxglove::DEFAULT_CAPABILITIES.end());
      if (useSimTime) {
        serverOptions.capabilities.push_back(::foxglove::CAPABILITY_TIME);
      }
      serverOptions.supportedEncodings = {"ros1"};
      serverOptions.metadata = {{"DISTRO", "basis"}};
      serverOptions.sendBufferLimitBytes = ::foxglove::DEFAULT_SEND_BUFFER_LIMIT_BYTES;
      serverOptions.sessionId = std::to_string(std::time(nullptr));
      serverOptions.useTls = false;
      serverOptions.certfile = "";
      serverOptions.keyfile = "";
      serverOptions.useCompression = false;
      serverOptions.clientTopicWhitelistPatterns = parseRegexPatterns({".*"});

      const auto logHandler =
          std::bind(&FoxgloveBridge::logHandler, this, std::placeholders::_1, std::placeholders::_2);

      server = ::foxglove::ServerFactory::createServer<ConnectionHandle>("foxglove_bridge", logHandler, serverOptions);
      ::foxglove::ServerHandlers<ConnectionHandle> hdlrs;
      hdlrs.subscribeHandler =
          std::bind(&FoxgloveBridge::subscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.unsubscribeHandler =
          std::bind(&FoxgloveBridge::unsubscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.clientAdvertiseHandler =
          std::bind(&FoxgloveBridge::clientAdvertise, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.clientUnadvertiseHandler =
          std::bind(&FoxgloveBridge::clientUnadvertise, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.clientMessageHandler =
          std::bind(&FoxgloveBridge::clientMessage, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.parameterRequestHandler = std::bind(&FoxgloveBridge::getParameters, this, std::placeholders::_1,
                                                std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameterChangeHandler = std::bind(&FoxgloveBridge::setParameters, this, std::placeholders::_1,
                                               std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameterSubscriptionHandler = std::bind(&FoxgloveBridge::subscribeParameters, this, std::placeholders::_1,
                                                     std::placeholders::_2, std::placeholders::_3);
      hdlrs.serviceRequestHandler =
          std::bind(&FoxgloveBridge::serviceRequest, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.subscribeConnectionGraphHandler = [](bool /*subscribe*/) { /*_subscribeGraphUpdates = subscribe;*/ };

      server->setHandlers(std::move(hdlrs));

      const std::string address = "0.0.0.0";
      const int port = 8765;

      server->start(address, static_cast<uint16_t>(port));
    } catch (const std::exception &err) {
      // TODO  LOG error ("Failed to start websocket server: %s", err.what());
      //   throw err;?
    }
  }

  ~FoxgloveBridge() {
    if (server) {
      server->stop();
    }
  }

private:
  void logHandler(::foxglove::WebSocketLogLevel level, [[maybe_unused]] char const *msg) {
    switch (level) {
    case ::foxglove::WebSocketLogLevel::Debug:
      // ROS_DEBUG("[WS] %s", msg);
      break;
    case ::foxglove::WebSocketLogLevel::Info:
      // ROS_INFO("[WS] %s", msg);
      break;
    case ::foxglove::WebSocketLogLevel::Warn:
      // ROS_WARN("[WS] %s", msg);
      break;
    case ::foxglove::WebSocketLogLevel::Error:
      // ROS_ERROR("[WS] %s", msg);
      break;
    case ::foxglove::WebSocketLogLevel::Critical:
      // ROS_FATAL("[WS] %s", msg);
      break;
    }
  }

  // Subscribe handler
  // @param channelId Channel ID
  // @param clientHandle Connection handle
  void subscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle) {
    std::lock_guard<std::mutex> lock(_subscriptionsMutex);

    auto it = _advertisedTopics.find(channelId);
    if (it == _advertisedTopics.end()) {
      const std::string errMsg = "Received subscribe request for unknown channel " + std::to_string(channelId);
      BASIS_LOG_WARN(errMsg);
      return;
    }
    const auto &channel = it->second;

    auto [subscriptionsIt, firstSubscription] = _subscriptions.emplace(channelId, SubscriptionsByClient());
    auto &subscriptionsByClient = subscriptionsIt->second;

    if (!firstSubscription && subscriptionsByClient.find(clientHandle) != subscriptionsByClient.end()) {
      const std::string errMsg = "Client is already subscribed to channel " + std::to_string(channelId);
      BASIS_LOG_WARN(errMsg);
      return;
    }

    try {
      std::shared_ptr<basis::core::transport::SubscriberBase> sub =
          transport_manager->SubscribeRaw(channel.topic,
                                          [&]([[maybe_unused]] auto msg) {
                                            // TODO
                                          },
                                          &thread_pool, nullptr, {});
      subscriptionsByClient.emplace(clientHandle, sub);
      if (firstSubscription) {
        BASIS_LOG_INFO("Subscribed to topic \"%s\" (%s) on channel %d", channel.topic.c_str(),
                       channel.schemaName.c_str(), channelId);
      } else {
        BASIS_LOG_INFO("Added subscriber #%zu to topic \"%s\" (%s) on channel %d", subscriptionsByClient.size(),
                       channel.topic.c_str(), channel.schemaName.c_str(), channelId);
      }
    } catch (const std::exception &ex) {
      const std::string errMsg =
          "Failed to subscribe to topic '" + channel.topic + "' (" + channel.schemaName + "): " + ex.what();
      BASIS_LOG_ERROR(errMsg);
    }
  }

  void unsubscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle) {
    std::lock_guard<std::mutex> lock(_subscriptionsMutex);

    const auto channelIt = _advertisedTopics.find(channelId);
    if (channelIt == _advertisedTopics.end()) {
      const std::string errMsg = "Received unsubscribe request for unknown channel " + std::to_string(channelId);
      BASIS_LOG_WARN(errMsg);
      return;
    }
    const auto &channel = channelIt->second;

    auto subscriptionsIt = _subscriptions.find(channelId);
    if (subscriptionsIt == _subscriptions.end()) {
      BASIS_LOG_ERROR("Received unsubscribe request for channel " + std::to_string(channelId) +
                      " that was not subscribed to ");
      return;
    }

    auto &subscriptionsByClient = subscriptionsIt->second;
    const auto clientSubscription = subscriptionsByClient.find(clientHandle);
    if (clientSubscription == subscriptionsByClient.end()) {
      BASIS_LOG_ERROR("Received unsubscribe request for channel " + std::to_string(channelId) +
                      "from a client that was not subscribed to this channel");
      return;
    }

    subscriptionsByClient.erase(clientSubscription);
    if (subscriptionsByClient.empty()) {
      BASIS_LOG_INFO("Unsubscribing from topic \"%s\" (%s) on channel %d", channel.topic.c_str(),
                     channel.schemaName.c_str(), channelId);
      _subscriptions.erase(subscriptionsIt);
    } else {
      BASIS_LOG_INFO("Removed one subscription from channel %d (%zu subscription(s) left)", channelId,
                     subscriptionsByClient.size());
    }
  }

  void clientAdvertise(const ::foxglove::ClientAdvertisement &channel, ConnectionHandle clientHandle) {
    // TODO: other encodings
    if (channel.encoding != "ros1") {
      BASIS_LOG_ERROR("Unsupported encoding. Only '" + std::string("ros1") + "' encoding is supported at the moment.");
      return;
    }

    std::unique_lock<std::shared_mutex> lock(_publicationsMutex);

    // Get client publications or insert an empty map.
    auto [clientPublicationsIt, isFirstPublication] =
        _clientAdvertisedTopics.emplace(clientHandle, ClientPublications());

    auto &clientPublications = clientPublicationsIt->second;
    if (!isFirstPublication && clientPublications.find(channel.channelId) != clientPublications.end()) {
      BASIS_LOG_ERROR("Received client advertisement from " + server->remoteEndpointString(clientHandle) +
                      " for channel " + std::to_string(channel.channelId) + " it had already advertised");
      return;
    }

    // transport_manager->GetSchemaManager();
    core::serialization::MessageSchema basis_schema;
    core::serialization::MessageTypeInfo message_type;
    std::shared_ptr<basis::core::transport::PublisherRaw> publisher =
        transport_manager->AdvertiseRaw(channel.topic, message_type, basis_schema);

    if (publisher) {
      clientPublications.insert({channel.channelId, std::move(publisher)});
      BASIS_LOG_INFO("Client %s is advertising \"%s\" (%s) on channel %d",
                     server->remoteEndpointString(clientHandle).c_str(), channel.topic.c_str(),
                     channel.schemaName.c_str(), channel.channelId);
      // TODO:
      // updateAdvertisedTopics();
    } else {
      const auto errMsg = "Failed to create publisher for topic " + channel.topic + "(" + channel.schemaName + ")";
      BASIS_LOG_ERROR(errMsg);
    }
  }

  void clientUnadvertise(::foxglove::ClientChannelId channelId, ConnectionHandle clientHandle) {
    std::unique_lock<std::shared_mutex> lock(_publicationsMutex);

    auto clientPublicationsIt = _clientAdvertisedTopics.find(clientHandle);
    if (clientPublicationsIt == _clientAdvertisedTopics.end()) {
      BASIS_LOG_ERROR("Ignoring client unadvertisement from " + server->remoteEndpointString(clientHandle) +
                      " for unknown channel " + std::to_string(channelId) + ", client has no advertised topics");
      return;
    }

    auto &clientPublications = clientPublicationsIt->second;

    auto channelPublicationIt = clientPublications.find(channelId);
    if (channelPublicationIt == clientPublications.end()) {
      BASIS_LOG_ERROR("Ignoring client unadvertisement from " + server->remoteEndpointString(clientHandle) +
                      " for unknown channel " + std::to_string(channelId) + ", client has " +
                      std::to_string(clientPublications.size()) + " advertised topic(s)");
      return;
    }

    const auto &publisher = channelPublicationIt->second;
    BASIS_LOG_INFO("Client %s is no longer advertising %s on channel %d",
                   server->remoteEndpointString(clientHandle).c_str(), publisher->GetPublisherInfo().topic.c_str(),
                   channelId);
    clientPublications.erase(channelPublicationIt);

    if (clientPublications.empty()) {
      _clientAdvertisedTopics.erase(clientPublicationsIt);
    }
  }

  void clientMessage([[maybe_unused]] const ::foxglove::ClientMessage &clientMsg,
                     [[maybe_unused]] ConnectionHandle clientHandle) {}

  void getParameters([[maybe_unused]] const std::vector<std::string> &parameters,
                     [[maybe_unused]] const std::optional<std::string> &requestId,
                     [[maybe_unused]] ConnectionHandle hdl) {}

  void setParameters([[maybe_unused]] const std::vector<::foxglove::Parameter> &parameters,
                     [[maybe_unused]] const std::optional<std::string> &requestId,
                     [[maybe_unused]] ConnectionHandle hdl) {}

  void subscribeParameters([[maybe_unused]] const std::vector<std::string> &parameters,
                           [[maybe_unused]] ::foxglove::ParameterSubscriptionOperation op,
                           [[maybe_unused]] ConnectionHandle) {}

  void serviceRequest([[maybe_unused]] const ::foxglove::ServiceRequest &request,
                      [[maybe_unused]] ConnectionHandle clientHandle) {}

  std::unique_ptr<::foxglove::ServerInterface<ConnectionHandle>> server;
  bool useSimTime = false;

  std::mutex _subscriptionsMutex;
  std::shared_mutex _publicationsMutex;

  std::unordered_map<::foxglove::ChannelId, ::foxglove::ChannelWithoutId> _advertisedTopics;
  std::unordered_map<::foxglove::ChannelId, SubscriptionsByClient> _subscriptions;

  PublicationsByClient _clientAdvertisedTopics;
};

} // namespace basis::plugins::bridges::foxglove