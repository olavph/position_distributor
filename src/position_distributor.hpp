#pragma once

#include "position.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <print>
#include <ranges>
#include <set>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class PositionDistributor {
public:
  PositionDistributor(asio::io_context &ioc, unsigned short port);

private:
  void start();
  void create_client_position(const asio::ip::tcp::endpoint &client_address,
                              const std::string &client_id);
  void update_client_position(const asio::ip::tcp::endpoint &client_address,
                              SymbolPosition position);

  // Session class to handle individual client connections
  class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(tcp::socket socket, PositionDistributor &distributor);
    void start();
    asio::ip::tcp::endpoint get_endpoint() const;
    void add_to_write_queue(const std::vector<unsigned char> &&data);

  private:
    void on_accept(beast::error_code ec);
    void on_read_header(beast::error_code ec, std::size_t bytes_transferred);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void process_write_queue();

    websocket::stream<tcp::socket> m_ws;
    PositionDistributor &m_distributor;
    // Buffer for incoming messages, which are read asynchronously.
    beast::flat_buffer m_buffer;
    // We cannot call async_write concurrently, so this queue allows the messages to be sequenced.
    std::deque<std::vector<unsigned char>> m_write_queue;
    bool m_write_in_progress{false};
  };

  tcp::acceptor m_acceptor;
  std::set<std::shared_ptr<Session>> m_sessions;
  std::map<asio::ip::tcp::endpoint, ClientPosition> m_client_positions;
};

// --- Definitions ---

PositionDistributor::PositionDistributor(asio::io_context &ioc, unsigned short port)
    : m_acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
  start();
}

void PositionDistributor::start() {
  m_acceptor.async_accept(asio::make_strand(m_acceptor.get_executor()),
                          [this](beast::error_code ec, tcp::socket socket) {
                            if (!ec) {
                              auto session = std::make_shared<Session>(std::move(socket), *this);
                              m_sessions.insert(session);
                              session->start();
                            }
                            start();
                          });
}

void PositionDistributor::create_client_position(const asio::ip::tcp::endpoint &client_address,
                                                 const std::string &client_id) {
  if (m_client_positions.count(client_address) == 0) {
    m_client_positions[client_address] = ClientPosition{client_id, {}};
    std::println("Created new client position for {}", client_id);
  }
}

void PositionDistributor::update_client_position(const asio::ip::tcp::endpoint &client_address,
                                                 SymbolPosition position) {
  auto &client_position = m_client_positions[client_address];
  client_position.m_positions[position.m_symbol] = position;
  position.m_symbol.append(".");
  position.m_symbol.append(client_position.m_client_id);
  std::println("Updated position for client {}: {} = {}", client_position.m_client_id,
               position.m_symbol, position.m_net_position);

  for (const auto &session : m_sessions) {
    if (session->get_endpoint() != client_address) {
      session->add_to_write_queue(position.to_bytes());
    }
  }
}

// --- Session Definitions ---

PositionDistributor::Session::Session(tcp::socket socket, PositionDistributor &distributor)
    : m_ws(std::move(socket)), m_distributor(distributor) {
  m_ws.binary(true);
}

void PositionDistributor::Session::start() {
  m_ws.async_accept(beast::bind_front_handler(&Session::on_accept, shared_from_this()));
}

asio::ip::tcp::endpoint PositionDistributor::Session::get_endpoint() const {
  return m_ws.next_layer().remote_endpoint();
}

void PositionDistributor::Session::add_to_write_queue(const std::vector<unsigned char> &&data) {
  asio::dispatch(m_ws.get_executor(),
                 [self = shared_from_this(), data = std::move(data)]() mutable {
                   self->m_write_queue.emplace_back(std::move(data));
                   if (!self->m_write_in_progress) {
                     self->m_write_in_progress = true;
                     self->process_write_queue();
                   }
                 });
}

void PositionDistributor::Session::on_accept(beast::error_code ec) {
  if (ec) {
    std::println(std::cerr, "Accept failed: {}", ec.message());
    return;
  }
  m_ws.async_read(m_buffer,
                  beast::bind_front_handler(&Session::on_read_header, shared_from_this()));
}

void PositionDistributor::Session::on_read_header(beast::error_code ec,
                                                  std::size_t bytes_transferred) {
  if (ec) {
    std::println(std::cerr, "Read header failed: {}", ec.message());
    return;
  }
  std::string client_id = beast::buffers_to_string(m_buffer.data());
  m_buffer.consume(bytes_transferred);
  auto client_endpoint = get_endpoint();
  m_distributor.create_client_position(client_endpoint, client_id);
  m_ws.async_read(m_buffer, beast::bind_front_handler(&Session::on_read, shared_from_this()));
}

void PositionDistributor::Session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    std::println(std::cerr, "Read failed: {}", ec.message());
    return;
  }
  auto req = m_buffer.data();
  auto message = std::ranges::subrange{buffers_begin(req), buffers_end(req)};
  auto position = SymbolPosition::from_bytes(message);
  m_buffer.consume(bytes_transferred);
  auto client_endpoint = get_endpoint();
  m_distributor.update_client_position(client_endpoint, position);
  m_ws.async_read(m_buffer, beast::bind_front_handler(&Session::on_read, shared_from_this()));
}

void PositionDistributor::Session::on_write(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    std::println(std::cerr, "Send failed: {}", ec.message());
    m_write_queue.pop_front();
    process_write_queue();
    return;
  }
  assert(!m_write_queue.empty());
  m_write_queue.pop_front();
  process_write_queue();
}

void PositionDistributor::Session::process_write_queue() {
  if (m_write_queue.empty()) {
    m_write_in_progress = false;
    return;
  }
  auto &message = m_write_queue.front();
  m_ws.async_write(asio::buffer(message),
                   beast::bind_front_handler(&Session::on_write, shared_from_this()));
}
