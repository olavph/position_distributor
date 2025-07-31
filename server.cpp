#include "src/position_distributor.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>

using namespace boost::asio;
using namespace boost::beast;

// Start the PositionDistributor server
int main() {
  try {
    asio::io_context ioc;
    unsigned short port = 9002; // Example port
    PositionDistributor distributor(ioc, port);

    std::cout << "Position Distributor started on port " << port << std::endl;

    ioc.run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
