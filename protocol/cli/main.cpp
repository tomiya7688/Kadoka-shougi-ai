#include "kadoka/position.hpp"

#include <iostream>

int main() {
    const auto position = kadoka::shogi::Position::startpos();
    std::cout << "Kadoka Shougi AI bootstrap\n";
    std::cout << position.to_sfen() << '\n';
    return 0;
}
