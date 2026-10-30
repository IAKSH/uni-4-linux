#include "cli.hpp"

#include <uni/version.hpp>

#include <CLI/CLI.hpp>
#include <format>

int main(int argc, char **argv) {
    CLI::App app{"Uniwill EC control for the MECHREVO Jiguang Pro (GM5AG0O)"};
    app.set_version_flag("-V,--version", std::format("uni {}", uni::version));
    app.require_subcommand(1);

    uni::cli::Options options;
    uni::cli::Commands commands;
    uni::cli::build_cli(app, options, commands);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
        return app.exit(error);
    }
    return uni::cli::dispatch(options, commands);
}
