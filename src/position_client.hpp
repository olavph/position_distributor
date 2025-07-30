#pragma once

#include "position.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;


class PositionClient : public std::enable_shared_from_this<PositionClient> {
public:
  PositionClient(asio::io_context &ioc, const std::string &host, unsigned short port,
                 const std::string &client_id);

  void connect();
  void send_position(const std::string_view symbol, double net_position);

private:
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
  void on_handshake(beast::error_code ec);
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_write(beast::error_code ec, std::size_t bytes_transferred);
  void add_to_write_queue(const std::vector<unsigned char> &&data);
  void process_write_queue();

  asio::strand<asio::io_context::executor_type> m_strand;
  tcp::resolver m_resolver;
  websocket::stream<beast::tcp_stream> m_ws;
  std::string m_host;
  unsigned short m_port;
  std::string m_client_id;

  beast::flat_buffer m_read_buffer;
  std::deque<std::vector<unsigned char>> m_write_queue;
  bool m_write_in_progress{false};

  std::map<std::string, ClientPosition> m_client_positions;
};

// --- Definitions ---

PositionClient::PositionClient(asio::io_context &ioc, const std::string &host, unsigned short port,
                 const std::string &client_id)
      : m_strand(asio::make_strand(ioc)), m_resolver(m_strand), m_ws(m_strand), m_host(host),
        m_port(port), m_client_id(client_id) {
    m_ws.binary(true);
}

void PositionClient::connect() {
    auto const port_str = std::to_string(m_port);
    m_resolver.async_resolve(
        m_host, port_str,
        beast::bind_front_handler(&PositionClient::on_resolve, shared_from_this()));
}

void PositionClient::send_position(const std::string_view symbol, double net_position) {
    std::vector<unsigned char> data(symbol.begin(), symbol.end());
    const unsigned char *pos_ptr = reinterpret_cast<const unsigned char *>(&net_position);
    data.insert(data.end(), pos_ptr, pos_ptr + sizeof(double));
    add_to_write_queue(std::move(data));
}

void PositionClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
      std::println(std::cerr, "Resolve failed: {}", ec.message());
      return;
    }
    beast::get_lowest_layer(m_ws).async_connect(
        results, beast::bind_front_handler(&PositionClient::on_connect, shared_from_this()));
}

void PositionClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    if (ec) {
      std::println(std::cerr, "Connect failed: {}", ec.message());
      return;
    }
    m_host += ':' + std::to_string(ep.port());
    std::println(std::cout, "Connected to: {}", m_host);
    m_ws.async_handshake(
        m_host, "/", beast::bind_front_handler(&PositionClient::on_handshake, shared_from_this()));
}

void PositionClient::on_handshake(beast::error_code ec) {
    if (ec) {
      std::println(std::cerr, "Handshake failed: {}", ec.message());
      return;
    }
    m_ws.async_read(m_read_buffer,
                    beast::bind_front_handler(&PositionClient::on_read, shared_from_this()));
    assert(!m_write_in_progress);
    add_to_write_queue(std::vector<unsigned char>(m_client_id.begin(), m_client_id.end()));
    std::println(std::cout, "Sent Client ID: {}", m_client_id);
}

void PositionClient::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
      std::println(std::cerr, "Read failed: {}", ec.message());
      return;
    }
    auto req = m_read_buffer.data();
    auto message = std::ranges::subrange{buffers_begin(req), buffers_end(req)};
    auto position = SymbolPosition::from_bytes(message);
    m_read_buffer.consume(bytes_transferred);
    size_t separator = position.m_symbol.find('.');
    if (separator == std::string::npos) {
      std::println(std::cerr, "Invalid position format: {}", position.m_symbol);
      return;
    }
    std::string client_id = position.m_symbol.substr(separator + 1);
    position.m_symbol = position.m_symbol.substr(0, separator);
    std::println(std::cout, "Received position from client {}: {} = {}", client_id, position.m_symbol, position.m_net_position);
    m_client_positions[client_id].m_positions[position.m_symbol] = position;
    m_ws.async_read(m_read_buffer,
                    beast::bind_front_handler(&PositionClient::on_read, shared_from_this()));
}

void PositionClient::on_write(beast::error_code ec, std::size_t bytes_transferred) {
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

void PositionClient::add_to_write_queue(const std::vector<unsigned char> &&data) {
    asio::dispatch(m_strand, [self = shared_from_this(), data = std::move(data)]() mutable {
      self->m_write_queue.emplace_back(std::move(data));
      if (!self->m_write_in_progress) {
        self->m_write_in_progress = true;
        self->process_write_queue();
      }
    });
}

void PositionClient::process_write_queue() {
    if (m_write_queue.empty()) {
      m_write_in_progress = false;
      return;
    }
    auto &message = m_write_queue.front();
    m_ws.async_write(asio::buffer(message),
                     beast::bind_front_handler(&PositionClient::on_write, shared_from_this()));
}
