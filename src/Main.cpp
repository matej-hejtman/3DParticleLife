#include "ParticleLife/App.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        std::filesystem::path root = std::filesystem::current_path();
        if (argc > 1) {
            root = argv[1];
        }
        pl::App app(root);
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
