#pragma once
#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>
#include <iostream>

std::optional<basis::core::transport::proto::MessageSchema>
FetchSchema(const std::string &schema_id, basis::core::transport::CoordinatorConnector *connector, int timeout_s) {
  connector->RequestSchemas({&schema_id, 1});

  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  while (std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    connector->Update();
    auto schema_ptr = connector->TryGetSchema(schema_id);
    if (schema_ptr) {
      return *schema_ptr;
    }

    if (connector->errors_from_coordinator.size()) {
      for (const std::string &error : connector->errors_from_coordinator) {
        std::cerr << "Errors when fetching schema:" << std::endl;
        std::cerr << error << std::endl;
      }
      return {};
    }
  }

  std::cerr << "Timed out while fetching schema [" << schema_id << "]" << std::endl;

  return {};
}
