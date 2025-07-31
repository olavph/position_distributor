#include "position_client.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <print>
#include <random>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace std::literals::string_view_literals;

static constexpr std::array symbols{"BTC"sv, "ETH"sv};

void send_periodic(PositionClient &client) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(-1.0, 1.0);

  std::vector<double> positions(symbols.size(), 0.0);

  while (true) {
    std::this_thread::sleep_for(2s);
    for (size_t i = 0; i < symbols.size(); ++i) {
      const auto &symbol = symbols[i];
      positions[i] += dis(gen);
      std::println("Sending: {} = {}", symbol, positions[i]);
      client.send_position(symbol, positions[i]);
    }
  }
}

// Connect a PositionClient to the server and send positions periodically,
// simulating updates over time.
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <client_id>" << std::endl;
    return 1;
  }
  std::string client_id = argv[1];
  boost::asio::io_context ioc;
  // Connect to a server on the same host
  auto client = std::make_shared<PositionClient>(ioc, "127.0.0.1", 9002, client_id);
  client->connect();
  std::thread sender([&] { send_periodic(*client); });
  ioc.run();
  sender.join();
  return 0;
}
