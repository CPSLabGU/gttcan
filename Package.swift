// swift-tools-version: 5.6

import PackageDescription

let package = Package(
    name: "gttcan",
    products: [
        .library(
            name: "gttcan",
            targets: ["gttcan"]),
    ],
    dependencies: [
    ],
    targets: [
        .target(
            name: "gttcan",
            dependencies: [],
            cSettings: [
                .unsafeFlags([
                    "-Wall",
                    "-Werror",
                    "-Wextra",
                    "-Wpedantic",
                    "-Wsign-conversion",
                    "-Wpointer-arith",
                    "-Wcast-qual",
                    "-Wstrict-prototypes",
                    "-Wmissing-prototypes"
                ])]),
        .testTarget(
            name: "gttcanTests",
            dependencies: ["gttcan"]),
    ]
)
