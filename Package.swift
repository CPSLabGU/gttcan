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
                    "-Weverything",
                    "-Wno-undef",
                    "-Wno-long-long",
                    "-Wno-c++98-compat-pedantic",
                    "-Wno-declaration-after-statement",
                    "-Wno-disabled-macro-expansion",
                    "-Wno-weak-vtables",
                    "-Wno-padded",
                    "-Wno-unknown-pragmas",
                    "-Wno-pedantic",
                    "-Wno-unknown-warning-option",
                    "-Wno-documentation-unknown-command"
                ])]),
        .testTarget(
            name: "gttcanTests",
            dependencies: ["gttcan"]),
    ]
)
