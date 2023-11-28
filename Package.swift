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
        .package(url: "https://github.com/apple/swift-docc-plugin", from: "1.1.0"),
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
