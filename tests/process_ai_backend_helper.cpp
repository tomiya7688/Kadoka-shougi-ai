#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::string parse_mode(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        constexpr const char prefix[] = "--mode=";
        if (argument.rfind(prefix, 0) == 0) {
            return argument.substr(sizeof(prefix) - 1);
        }
    }
    return {};
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = parse_mode(argc, argv);
    std::size_t request_count = 0;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.rfind("request ", 0) != 0) continue;

        const std::size_t request_id = static_cast<std::size_t>(
            std::stoull(line.substr(std::string("request ").size()))
        );
        ++request_count;

        bool mutual_impasse_offer = false;
        while (std::getline(std::cin, line) && line != "end") {
            if (line == "interaction mutual_impasse_offer") {
                mutual_impasse_offer = true;
            }
        }

        if (mode == "exit") {
            return 7;
        }
        if (mode == "timeout") {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "result " << request_id << '\n';
        if (mutual_impasse_offer) {
            if (mode == "accept_impasse") {
                std::cout << "agreement accept\n";
            } else if (mode == "malformed_agreement") {
                std::cout << "agreement maybe\n";
            } else {
                std::cout << "agreement decline\n";
            }
            std::cout << "end\n" << std::flush;
            continue;
        }

        if (mode == "malformed") {
            std::cout << "move normal invalid\n";
        } else if (mode == "illegal") {
            std::cout << "move normal 7 7 7 5 0\n";
        } else if (mode == "resign") {
            std::cout << "action resign\n";
        } else if (mode == "declare") {
            std::cout << "action declare_entering_king\n";
        } else if (mode == "offer_impasse") {
            std::cout << "action offer_mutual_impasse\n";
        } else {
            std::cout << "move normal 7 7 7 6 0\n";
        }
        std::cout << "score_cp 12\n";
        std::cout << "nodes 34\n";
        std::cout << "depth 2\n";
        std::cout << "info request_count=" << request_count << '\n';
        std::cout << "end\n" << std::flush;
    }

    return 0;
}
