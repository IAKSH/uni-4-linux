#include "cli.hpp"

#include <CLI/CLI.hpp>

#ifndef UNI_VERSION
#define UNI_VERSION "0.1.0"
#endif

int main(int argc, char **argv) {
    CLI::App app{"Uniwill EC control for the MECHREVO Jiguang Pro (GM5AG0O)"};
    app.set_version_flag("-V,--version", UNI_VERSION);
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
