
#include <basis/plugins/bridges/foxglove_bridge.h>

#include <foxglove/websocket/base64.hpp>
#include <foxglove/websocket/common.hpp>
#include <foxglove/websocket/regex_utils.hpp>
#include <foxglove/websocket/server_factory.hpp>

#include <basis/core/logging/macros.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport_manager.h>
#include <basis/plugins/transport/tcp.h>
#include <basis/plugins/transport/tcp_transport_name.h>
#include <basis/unit.h>

#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <unordered_map>

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

FoxgloveBridge::FoxgloveBridge(std::string unit_name) : SingleThreadedUnit(unit_name) {}

FoxgloveBridge::~FoxgloveBridge() {
  if (server) {
    server->stop();
  }
}

void FoxgloveBridge::Initialize([[maybe_unused]] const UnitInitializeOptions &options) { init(); }

void FoxgloveBridge::Update(const basis::core::Duration &max_sleep_duration) {
  SingleThreadedUnit::Update(max_sleep_duration);
  updateAdvertisedTopics();
}

void FoxgloveBridge::init(const std::string &address, int port) {

  using namespace basis::core::transport;
  using namespace basis::plugins::transport;

  transport_manager = std::make_unique<TransportManager>(std::make_unique<InprocTransport>());

  transport_manager->RegisterTransport(basis::plugins::transport::TCP_TRANSPORT_NAME, std::make_unique<TcpTransport>());

  try {
    ::foxglove::ServerOptions serverOptions;

    const std::vector<std::string> capabilities = {
        ::foxglove::CAPABILITY_CLIENT_PUBLISH,
    };

    serverOptions.capabilities = capabilities;
    if (useSimTime) {
      serverOptions.capabilities.push_back(::foxglove::CAPABILITY_TIME);
    }
    serverOptions.supportedEncodings = {"ros1", "protobuf"};
    serverOptions.metadata = {{"DISTRO", "basis"}};
    serverOptions.sendBufferLimitBytes = ::foxglove::DEFAULT_SEND_BUFFER_LIMIT_BYTES;
    serverOptions.sessionId = std::to_string(std::time(nullptr));
    serverOptions.useTls = false;
    serverOptions.certfile = "";
    serverOptions.keyfile = "";
    serverOptions.useCompression = false;
    serverOptions.clientTopicWhitelistPatterns = parseRegexPatterns({".*"});

    const auto logHandler = std::bind(&FoxgloveBridge::logHandler, this, std::placeholders::_1, std::placeholders::_2);

    server = ::foxglove::ServerFactory::createServer<ConnectionHandle>("foxglove_bridge", logHandler, serverOptions);
    ::foxglove::ServerHandlers<ConnectionHandle> hdlrs;
    hdlrs.subscribeHandler = std::bind(&FoxgloveBridge::subscribe, this, std::placeholders::_1, std::placeholders::_2);
    hdlrs.unsubscribeHandler =
        std::bind(&FoxgloveBridge::unsubscribe, this, std::placeholders::_1, std::placeholders::_2);
    hdlrs.clientAdvertiseHandler =
        std::bind(&FoxgloveBridge::clientAdvertise, this, std::placeholders::_1, std::placeholders::_2);
    hdlrs.clientUnadvertiseHandler =
        std::bind(&FoxgloveBridge::clientUnadvertise, this, std::placeholders::_1, std::placeholders::_2);
    hdlrs.clientMessageHandler =
        std::bind(&FoxgloveBridge::clientMessage, this, std::placeholders::_1, std::placeholders::_2);

    server->setHandlers(std::move(hdlrs));

    server->start(address, static_cast<uint16_t>(port));
  } catch (const std::exception &err) {
    BASIS_LOG_ERROR("Failed to start websocket server: %s", err.what());
  }
}

void FoxgloveBridge::logHandler(::foxglove::WebSocketLogLevel level, char const *msg) {
  switch (level) {
  case ::foxglove::WebSocketLogLevel::Debug:
    BASIS_LOG_DEBUG("[WS] {}", msg);
    break;
  case ::foxglove::WebSocketLogLevel::Info:
    BASIS_LOG_INFO("[WS] {}", msg);
    break;
  case ::foxglove::WebSocketLogLevel::Warn:
    BASIS_LOG_WARN("[WS] {}", msg);
    break;
  case ::foxglove::WebSocketLogLevel::Error:
    BASIS_LOG_ERROR("[WS] {}", msg);
    break;
  case ::foxglove::WebSocketLogLevel::Critical:
    BASIS_LOG_FATAL("[WS] {}", msg);
    break;
  }
}

void FoxgloveBridge::subscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  auto it = advertisedTopics.find(channelId);
  if (it == advertisedTopics.end()) {
    const std::string errMsg = "Received subscribe request for unknown channel " + std::to_string(channelId);
    BASIS_LOG_WARN(errMsg);
    return;
  }
  const auto &channel = it->second;

  auto [subscriptionsIt, firstSubscription] = subscriptions.emplace(channelId, SubscriptionsByClient());
  auto &subscriptionsByClient = subscriptionsIt->second;

  if (!firstSubscription && subscriptionsByClient.find(clientHandle) != subscriptionsByClient.end()) {
    const std::string errMsg = "Client is already subscribed to channel " + std::to_string(channelId);
    BASIS_LOG_WARN(errMsg);
    return;
  }

  try {
    std::shared_ptr<basis::core::transport::SubscriberBase> sub = transport_manager->SubscribeRaw(
        channel.topic,
        [&](auto msg) {
          const auto payload = msg->GetPayload();
          const uint64_t receiptTimeNs = msg->GetMessageHeader()->send_time;
          server->sendMessage(clientHandle, channelId, receiptTimeNs, reinterpret_cast<const uint8_t *>(payload.data()),
                              payload.size());
        },
        &thread_pool, nullptr, {});
    subscriptionsByClient.emplace(clientHandle, sub);
    if (firstSubscription) {
      BASIS_LOG_INFO("Subscribed to topic \"%s\" (%s) on channel %d", channel.topic.c_str(), channel.schemaName.c_str(),
                     channelId);
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

void FoxgloveBridge::unsubscribe(::foxglove::ChannelId channelId, ConnectionHandle clientHandle) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  const auto channelIt = advertisedTopics.find(channelId);
  if (channelIt == advertisedTopics.end()) {
    const std::string errMsg = "Received unsubscribe request for unknown channel " + std::to_string(channelId);
    BASIS_LOG_WARN(errMsg);
    return;
  }
  const auto &channel = channelIt->second;

  auto subscriptionsIt = subscriptions.find(channelId);
  if (subscriptionsIt == subscriptions.end()) {
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
    subscriptions.erase(subscriptionsIt);
  } else {
    BASIS_LOG_INFO("Removed one subscription from channel %d (%zu subscription(s) left)", channelId,
                   subscriptionsByClient.size());
  }
}

void FoxgloveBridge::clientAdvertise(const ::foxglove::ClientAdvertisement &channel, ConnectionHandle clientHandle) {
  // TODO: other encodings?
  if (channel.encoding != "ros1" && channel.encoding != "protobuf") {
    BASIS_LOG_ERROR("Unsupported encoding. Only 'ros1' and 'protobuf' encodings are supported at the moment.");
    return;
  }

  std::unique_lock<std::shared_mutex> lock(publicationsMutex);

  // Get client publications or insert an empty map.
  auto [clientPublicationsIt, isFirstPublication] = clientAdvertisedTopics.emplace(clientHandle, ClientPublications());

  auto &clientPublications = clientPublicationsIt->second;
  if (!isFirstPublication && clientPublications.find(channel.channelId) != clientPublications.end()) {
    BASIS_LOG_ERROR("Received client advertisement from " + server->remoteEndpointString(clientHandle) +
                    " for channel " + std::to_string(channel.channelId) + " it had already advertised");
    return;
  }

  core::serialization::MessageTypeInfo message_type;
  message_type.serializer = channel.encoding;
  message_type.name = channel.schemaName;

  basis::core::transport::SchemaManager &schema_manager = transport_manager->GetSchemaManager();
  auto const &schemas = schema_manager.GetRegisteredSchemas();
  auto schemaIt = schemas.find(message_type.SchemaId());
  if (schemaIt == schemas.end()) {
    BASIS_LOG_ERROR("Unknown schema " + message_type.SchemaId());
    return;
  }

  std::shared_ptr<basis::core::transport::PublisherRaw> publisher =
      transport_manager->AdvertiseRaw(channel.topic, message_type, schemaIt->second);

  if (publisher) {
    clientPublications.insert({channel.channelId, std::move(publisher)});
    BASIS_LOG_INFO("Client %s is advertising \"%s\" (%s) on channel %d",
                   server->remoteEndpointString(clientHandle).c_str(), channel.topic.c_str(),
                   channel.schemaName.c_str(), channel.channelId);

    updateAdvertisedTopics();
  } else {
    const auto errMsg = "Failed to create publisher for topic " + channel.topic + "(" + channel.schemaName + ")";
    BASIS_LOG_ERROR(errMsg);
  }
}

void FoxgloveBridge::clientUnadvertise(::foxglove::ClientChannelId channelId, ConnectionHandle clientHandle) {
  std::unique_lock<std::shared_mutex> lock(publicationsMutex);

  auto clientPublicationsIt = clientAdvertisedTopics.find(clientHandle);
  if (clientPublicationsIt == clientAdvertisedTopics.end()) {
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
    clientAdvertisedTopics.erase(clientPublicationsIt);
  }
}

static std::string getSerializerFromSchemaId(const std::string &schemaId) {
  if (schemaId.starts_with("rosmsg:"))
    return "ros1";

  if (schemaId.starts_with("protobuf:"))
    return "protobuf";

  return "";
}

void FoxgloveBridge::clientMessage(const ::foxglove::ClientMessage &clientMsg, ConnectionHandle clientHandle) {
  const auto channelId = clientMsg.advertisement.channelId;
  std::shared_lock<std::shared_mutex> lock(publicationsMutex);

  auto clientPublicationsIt = clientAdvertisedTopics.find(clientHandle);
  if (clientPublicationsIt == clientAdvertisedTopics.end()) {
    BASIS_LOG_ERROR("Dropping client message from " + server->remoteEndpointString(clientHandle) +
                    " for unknown channel " + std::to_string(channelId) + ", client has no advertised topics");
    return;
  }

  auto &clientPublications = clientPublicationsIt->second;

  auto channelPublicationIt = clientPublications.find(clientMsg.advertisement.channelId);
  if (channelPublicationIt == clientPublications.end()) {
    BASIS_LOG_ERROR("Dropping client message from " + server->remoteEndpointString(clientHandle) +
                    " for unknown channel " + std::to_string(channelId) + ", client has " +
                    std::to_string(clientPublications.size()) + " advertised topic(s)");
    return;
  }

  auto packet = std::make_shared<core::transport::MessagePacket>(core::transport::MessageHeader::DataType::MESSAGE,
                                                                 clientMsg.getLength());
  memcpy(packet->GetMutablePayload().data(), clientMsg.getData(), clientMsg.getLength());
  basis::core::MonotonicTime now = basis::core::MonotonicTime::Now();
  channelPublicationIt->second->PublishRaw(packet, now);
}

void FoxgloveBridge::updateAdvertisedTopics() {

  auto info = coordinator_connector->GetLastNetworkInfo();
  if (!info) {
    BASIS_LOG_WARN("Failed to retrieve published topics from Coordinator.");
    return;
  }

  std::unordered_set<TopicAndDatatype, PairHash> latestTopics;
  latestTopics.reserve(info->publishers_by_topic_size());
  for (const auto &[topic_name, publishers] : info->publishers_by_topic()) {
    if (publishers.publishers_size()) {
      auto topic_name = publishers.publishers(0).topic();
      if (::foxglove::isWhitelisted(topic_name, topicWhitelistPatterns)) {
        latestTopics.emplace(topic_name, publishers.publishers(0).schema_id());
      }
    }
  }

  if (const auto numIgnoredTopics = info->publishers_by_topic_size() - latestTopics.size()) {
    BASIS_LOG_DEBUG("%zu topics have been ignored as they do not match any pattern on the topic whitelist",
                    numIgnoredTopics);
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // Remove channels for which the topic does not exist anymore
  std::vector<::foxglove::ChannelId> channelIdsToRemove;
  for (auto channelIt = advertisedTopics.begin(); channelIt != advertisedTopics.end();) {
    const TopicAndDatatype topicAndDatatype = {channelIt->second.topic, channelIt->second.schemaName};
    if (latestTopics.find(topicAndDatatype) == latestTopics.end()) {
      const auto channelId = channelIt->first;
      channelIdsToRemove.push_back(channelId);
      subscriptions.erase(channelId);
      BASIS_LOG_DEBUG("Removed channel %d for topic \"%s\" (%s)", channelId, topicAndDatatype.first.c_str(),
                      topicAndDatatype.second.c_str());
      channelIt = advertisedTopics.erase(channelIt);
    } else {
      channelIt++;
    }
  }
  server->removeChannels(channelIdsToRemove);

  // Add new channels for new topics
  std::vector<::foxglove::ChannelWithoutId> channelsToAdd;
  for (const auto &topicAndDatatype : latestTopics) {
    if (std::find_if(advertisedTopics.begin(), advertisedTopics.end(),
                     [topicAndDatatype](const auto &channelIdAndChannel) {
                       const auto &channel = channelIdAndChannel.second;
                       return channel.topic == topicAndDatatype.first && channel.schemaName == topicAndDatatype.second;
                     }) != advertisedTopics.end()) {
      continue; // Topic already advertised
    }

    ::foxglove::ChannelWithoutId newChannel{};
    newChannel.topic = topicAndDatatype.first;
    newChannel.schemaName = topicAndDatatype.second;
    newChannel.encoding = getSerializerFromSchemaId(topicAndDatatype.second);
    if (newChannel.encoding.empty()) {
      BASIS_LOG_ERROR("Could not find serializer for schema %s", topicAndDatatype.second.c_str());
      continue;
    }

    const auto msgDescription = coordinator_connector->TryGetSchema(topicAndDatatype.second);

    if (msgDescription) {
      newChannel.schema = msgDescription->schema();
      channelsToAdd.push_back(newChannel);
      BASIS_LOG_INFO("topic: {} schemaName: {} encoding: {} schema: {}", newChannel.topic, newChannel.schemaName, newChannel.encoding, newChannel.schema);
    } else {
      const std::string schemaId = topicAndDatatype.second;
      coordinator_connector->RequestSchemas({&schemaId, 1});

      BASIS_LOG_WARN("Could not find definition for type {}", topicAndDatatype.second.c_str());
    }
  }

  const auto channelIds = server->addChannels(channelsToAdd);
  for (size_t i = 0; i < channelsToAdd.size(); ++i) {
    const auto channelId = channelIds[i];
    const auto &channel = channelsToAdd[i];
    advertisedTopics.emplace(channelId, channel);
    BASIS_LOG_DEBUG("Advertising channel %d for topic \"%s\" (%s)", channelId, channel.topic.c_str(),
                    channel.schemaName.c_str());
  }
}

} // namespace basis::plugins::bridges::foxglove

extern "C" {

basis::Unit *CreateUnit(std::string unit_name) {
  return new basis::plugins::bridges::foxglove::FoxgloveBridge(unit_name);
}
}