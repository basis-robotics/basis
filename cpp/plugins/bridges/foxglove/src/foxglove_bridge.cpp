
#include <basis/plugins/bridges/foxglove_bridge.h>

#include <foxglove/websocket/base64.hpp>
#include <foxglove/websocket/common.hpp>
#include <foxglove/websocket/regex_utils.hpp>
#include <foxglove/websocket/server_factory.hpp>

#include <google/protobuf/descriptor.pb.h>

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

FoxgloveBridge::FoxgloveBridge(const std::optional<std::string_view> &unit_name)
    : SingleThreadedUnit(unit_name.value_or("FoxgloveBridge")) {}

FoxgloveBridge::~FoxgloveBridge() {
  if (server) {
    server->stop();
  }
}

void FoxgloveBridge::Initialize([[maybe_unused]] const UnitInitializeOptions &options) { init(); }

void FoxgloveBridge::Update(std::atomic<bool> *stop_token, const basis::core::Duration &max_sleep_duration) {
  SingleThreadedUnit::Update(stop_token, max_sleep_duration);
  updateAdvertisedTopics();
}

void FoxgloveBridge::init(const std::string &address, int port) {

  using namespace basis::core::transport;
  using namespace basis::plugins::transport;

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
    BASIS_LOG_FATAL("Failed to start websocket server: {}", err.what());
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
    std::shared_ptr<basis::core::transport::SubscriberBase> sub =
        transport_manager->SubscribeRaw(channel.topic,
                                        [this, clientHandle, channelId](auto msg) {
                                          const uint64_t receiptTimeNs = msg->GetMessageHeader()->send_time;
                                          const auto &payload = msg->GetPayload();
                                          const void *data = payload.data();
                                          server->sendMessage(clientHandle, channelId, receiptTimeNs,
                                                              static_cast<const uint8_t *>(data), payload.size());
                                        },
                                        &thread_pool, nullptr, {});

    subscriptionsByClient.emplace(clientHandle, sub);
    if (firstSubscription) {
      BASIS_LOG_INFO("Subscribed to topic \"{}\" ({}) on channel {}", channel.topic.c_str(), channel.schemaName.c_str(),
                     channelId);
    } else {
      BASIS_LOG_INFO("Added subscriber #{} to topic \"{}\" ({}) on channel {}", subscriptionsByClient.size(),
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
    BASIS_LOG_INFO("Unsubscribing from topic \"{}\" ({}) on channel {}", channel.topic.c_str(),
                   channel.schemaName.c_str(), channelId);
    subscriptions.erase(subscriptionsIt);
  } else {
    BASIS_LOG_INFO("Removed one subscription from channel {} ({} subscription(s) left)", channelId,
                   subscriptionsByClient.size());
  }
}

void FoxgloveBridge::clientAdvertise(const ::foxglove::ClientAdvertisement &channel, ConnectionHandle clientHandle) {

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
  auto schema_message = schema_manager.TryGetSchema(message_type.SchemaId());
  if (!schema_message) {
    BASIS_LOG_ERROR("Failed to retrieve schema for {}", message_type.SchemaId());
    return;
  }

  std::shared_ptr<basis::core::transport::PublisherRaw> publisher =
      transport_manager->AdvertiseRaw(channel.topic, message_type, *schema_message);

  if (publisher) {
    clientPublications.insert({channel.channelId, std::move(publisher)});
    BASIS_LOG_INFO("Client {} is advertising \"{}\" ({}) on channel {}",
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
  BASIS_LOG_INFO("Client {} is no longer advertising {} on channel {}",
                 server->remoteEndpointString(clientHandle).c_str(), publisher->GetPublisherInfo().topic.c_str(),
                 channelId);
  clientPublications.erase(channelPublicationIt);

  if (clientPublications.empty()) {
    clientAdvertisedTopics.erase(clientPublicationsIt);
  }
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

std::pair<std::string, std::string> split_serializer_and_schema(std::string name) {
  auto pos = name.find(':');
  return {name.substr(0, pos), name.substr(pos + 1)};
}

void FoxgloveBridge::updateAdvertisedTopics() {

  auto info = coordinator_connector->GetLastNetworkInfo();
  if (!info) {
    BASIS_LOG_WARN("Failed to retrieve published topics from Coordinator.");
    return;
  }

  std::vector<TopicAndDatatype> latestTopics;
  latestTopics.reserve(info->publishers_by_topic_size());
  for (const auto &[topic_name, publishers] : info->publishers_by_topic()) {
    if (publishers.publishers_size()) {
      auto topic_name = publishers.publishers(0).topic();
      if (::foxglove::isWhitelisted(topic_name, topicWhitelistPatterns)) {
        std::string schema = publishers.publishers(0).schema_id();
        auto [serializer, schema_name] = split_serializer_and_schema(schema);
        const TopicAndDatatype topicAndDatatype = {topic_name, serializer, schema_name};
        latestTopics.push_back(topicAndDatatype);
      }
    }
  }

  if (const auto numIgnoredTopics = info->publishers_by_topic_size() - latestTopics.size()) {
    BASIS_LOG_DEBUG("{} topics have been ignored as they do not match any pattern on the topic whitelist",
                    numIgnoredTopics);
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // Remove channels for which the topic does not exist anymore
  std::vector<::foxglove::ChannelId> channelIdsToRemove;
  for (auto channelIt = advertisedTopics.begin(); channelIt != advertisedTopics.end();) {
    if (std::find_if(latestTopics.begin(), latestTopics.end(), [channelIt](const auto &topicAndDatatype) {
          const auto &channel = channelIt->second;
          return channel.topic == topicAndDatatype.topic && channel.schemaName == topicAndDatatype.schema;
        }) != latestTopics.end()) {
      channelIt++;
    } else {
      const auto channelId = channelIt->first;
      channelIdsToRemove.push_back(channelId);
      subscriptions.erase(channelId);
      BASIS_LOG_INFO("Removed channel {} for topic \"{}\"", channelId, channelIt->second.schema);
      channelIt = advertisedTopics.erase(channelIt);
    }
  }
  server->removeChannels(channelIdsToRemove);

  // Add new channels for new topics
  std::vector<::foxglove::ChannelWithoutId> channelsToAdd;
  for (const auto &topicAndDatatype : latestTopics) {
    if (topicAndDatatype.schema_serializer == "raw") {
      // For now - skip any raw channels, later it would be good to work with Foxglove to show them as existing but
      // unsubscribable
      continue;
    }
    if (std::find_if(advertisedTopics.begin(), advertisedTopics.end(),
                     [topicAndDatatype](const auto &channelIdAndChannel) {
                       const auto &channel = channelIdAndChannel.second;
                       return channel.topic == topicAndDatatype.topic && channel.schemaName == topicAndDatatype.schema;
                     }) != advertisedTopics.end()) {
      continue; // Topic already advertised
    }

    ::foxglove::ChannelWithoutId newChannel{};
    newChannel.topic = topicAndDatatype.topic;
    newChannel.schemaName = topicAndDatatype.schema;
    newChannel.encoding = topicAndDatatype.schema_serializer;
    newChannel.schemaEncoding = newChannel.encoding;
    BASIS_LOG_INFO("New channel  --  topic: {} schemaName: {} encoding: {}", newChannel.topic, newChannel.schemaName,
                   newChannel.encoding);

    if (newChannel.encoding.empty()) {
      BASIS_LOG_ERROR("Could not find serializer for schema {}", topicAndDatatype.schema);
      continue;
    }

    const std::string schemaId = topicAndDatatype.schema_serializer + ":" + topicAndDatatype.schema;
    const auto msgDescription = coordinator_connector->TryGetSchema(schemaId);

    if (!msgDescription) {
      BASIS_LOG_INFO("Could not find definition for type {}, requesting from Coordinator...", topicAndDatatype.schema);

      coordinator_connector->RequestSchemas({&schemaId, 1});
      continue;
    }

    std::string schema =
        msgDescription->schema_efficient().empty() ? msgDescription->schema() : msgDescription->schema_efficient();
    newChannel.schema = ::foxglove::base64Encode(schema);
    channelsToAdd.push_back(newChannel);
    BASIS_LOG_DEBUG("newChannel -- topic: {} schemaName: {} encoding: {} schema: {}", newChannel.topic,
                    newChannel.schemaName, newChannel.encoding, newChannel.schema);
  }

  const auto channelIds = server->addChannels(channelsToAdd);
  for (size_t i = 0; i < channelsToAdd.size(); ++i) {
    const auto channelId = channelIds[i];
    const auto &channel = channelsToAdd[i];
    advertisedTopics.emplace(channelId, channel);
    BASIS_LOG_INFO("Advertising channel {} for topic \"{}\" ({})", channelId, channel.topic.c_str(),
                   channel.schemaName.c_str());
  }
}

} // namespace basis::plugins::bridges::foxglove

extern "C" {

basis::Unit *CreateUnit(const std::optional<std::string_view> &unit_name_override,
                        [[maybe_unused]] const basis::arguments::CommandLineTypes &command_line) {
  // TODO: create args for Foxglove

  return new basis::plugins::bridges::foxglove::FoxgloveBridge(unit_name_override);
}
}