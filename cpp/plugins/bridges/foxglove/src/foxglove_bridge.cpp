#include <foxglove_bridge/common.hpp>
#include <foxglove_bridge/foxglove_bridge.hpp>
// #include <foxglove_bridge/param_utils.hpp>
#include <foxglove_bridge/server_factory.hpp>
#include <foxglove_bridge/server_interface.hpp>
#include <foxglove_bridge/websocket_server.hpp>

#include <basis/core/logging/macros.h>

#include <memory>

using ConnectionHandle = websocketpp::connection_hdl;

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

class FoxgloveBridge {
public:
  void init() {
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

  void subscribe([[maybe_unused]] ::foxglove::ChannelId channelId, [[maybe_unused]] ConnectionHandle clientHandle) {}

  void unsubscribe([[maybe_unused]] ::foxglove::ChannelId channelId, [[maybe_unused]] ConnectionHandle clientHandle) {}

  void clientAdvertise([[maybe_unused]] const ::foxglove::ClientAdvertisement &channel,
                       [[maybe_unused]] ConnectionHandle clientHandle) {}

  void clientUnadvertise([[maybe_unused]] ::foxglove::ClientChannelId channelId,
                         [[maybe_unused]] ConnectionHandle clientHandle) {}

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
};

} // namespace basis::plugins::bridges::foxglove