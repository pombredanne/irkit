#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#define private public
#define protected public
#include "irkit/prefixmap.hpp"

namespace fs = boost::filesystem;

void run_build(const std::string& input, const std::string& output)
{
    auto map = irk::build_prefix_map_from_file<std::size_t>(input);
    //auto encoded = map.codec_->encode("00");
    //std::cout << encoded << std::endl;
    //auto[exact, block_node] = map.block_leaders_.find_or_first_lower(encoded);
    //std::cout << "exact: " << exact << ", " << block_node->value.value_or(17)
    //          << std::endl;
    auto idx = map["000000000100"];
    if (idx.has_value()) {
        std::cout << "Found: " << idx.value() << std::endl;
    } else {
        std::cout << "Not found" << std::endl;
    }
    irk::io::dump(map, output);
}

void run_lookup(const std::string& map_file, const std::string& key)
{
    auto map = irk::load_prefix_map<std::size_t>(map_file);
    //auto encoded = map.codec_->encode(key);
    //std::cout << encoded << std::endl;
    //auto[exact, block_node] = map.block_leaders_.find_or_first_lower(encoded);
    //std::cout << "exact: " << exact << ", " << block_node->value.value_or(17)
    //          << std::endl;
    //auto map = irk::build_prefix_map_from_file<std::size_t>(map_file);
    auto idx = map[key];
    if (idx.has_value()) {
        std::cout << idx.value() << std::endl;
    } else {
        std::cout << "Not found" << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::string input, map_file, string_key;

    CLI::App app{"irk-prefmap"};
    app.require_subcommand(1);

    CLI::App* build = app.add_subcommand("build", "Build a prefix map");
    build->add_option("input", input, "input file", false)
        ->check(CLI::ExistingFile)
        ->required();
    build->add_option("output", map_file, "output", false)->required();

    CLI::App* lookup =
        app.add_subcommand("lookup", "Resolve ID of a string in a map.");
    lookup->add_option("map", map_file, "map file", false)
        ->required()
        ->check(CLI::ExistingFile);
    lookup->add_option("string-key", string_key, "A string key to resolve")
        ->required();

    CLI11_PARSE(app, argc, argv);

    if (*lookup) {
        run_lookup(map_file, string_key);
        return 0;
    }

    if (*build) {
        run_build(input, map_file);
        return 0;
    }
}
