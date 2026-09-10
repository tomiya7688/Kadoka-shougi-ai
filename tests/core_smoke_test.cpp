#include "kadoka/position.hpp"

#include <cassert>
#include <string>

int main() {
    const auto position = kadoka::shogi::Position::startpos();

    assert(position.side_to_move() == kadoka::shogi::Color::Black);
    assert(position.ply() == 1);
    assert(position.to_sfen() ==
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    return 0;
}
