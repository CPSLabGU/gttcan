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
            dependencies: []),
        .testTarget(
            name: "gttcanTests",
            dependencies: ["gttcan"]),
    ]
)
