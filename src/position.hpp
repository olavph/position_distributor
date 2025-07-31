#pragma once

#include <cassert>
#include <map>
#include <string>
#include <vector>


struct SymbolPosition {
  static constexpr int MIN_SYMBOL_LENGTH = 3;
  static constexpr int MAX_SYMBOL_LENGTH = 8;


  SymbolPosition();
  SymbolPosition(const std::string &symbol, double net_position);
  const std::vector<unsigned char> to_bytes() const;
  static SymbolPosition from_bytes(auto &bytes);

  std::string m_symbol;
  double m_net_position;
};

struct ClientPosition {
  std::string m_client_id;
  std::map<std::string, SymbolPosition> m_positions;
};

// --- Definitions ---

SymbolPosition::SymbolPosition() = default;

SymbolPosition::SymbolPosition(const std::string &symbol, double net_position)
    : m_symbol(symbol), m_net_position(net_position) {}

const std::vector<unsigned char> SymbolPosition::to_bytes() const {
    std::vector<unsigned char> data(m_symbol.begin(), m_symbol.end());
    const unsigned char *pos_ptr = reinterpret_cast<const unsigned char *>(&m_net_position);
    data.insert(data.end(), pos_ptr, pos_ptr + sizeof(double));
    return data;
}

SymbolPosition SymbolPosition::from_bytes(auto &bytes) {
    assert(bytes.size() >= MIN_SYMBOL_LENGTH + sizeof(double) &&
           bytes.size() <= MAX_SYMBOL_LENGTH + sizeof(double));

    SymbolPosition position;
    position.m_symbol = std::string(bytes.begin(), bytes.end() - sizeof(double));
    position.m_net_position =
        *reinterpret_cast<const double *>(&(*(bytes.begin() + position.m_symbol.size())));
    return position;
}
