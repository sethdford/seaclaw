// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "SeaClawKit",
    platforms: [
        .macOS(.v14),
        .iOS(.v17)
    ],
    products: [
        .library(name: "SeaClawProtocol", targets: ["SeaClawProtocol"]),
        .library(name: "SeaClawClient", targets: ["SeaClawClient"]),
        .library(name: "SeaClawChatUI", targets: ["SeaClawChatUI"]),
    ],
    targets: [
        .target(
            name: "SeaClawProtocol",
            path: "Sources/SeaClawProtocol"
        ),
        .target(
            name: "SeaClawClient",
            dependencies: ["SeaClawProtocol"],
            path: "Sources/SeaClawClient"
        ),
        .target(
            name: "SeaClawChatUI",
            dependencies: ["SeaClawProtocol"],
            path: "Sources/SeaClawChatUI"
        ),
        .testTarget(
            name: "SeaClawProtocolTests",
            dependencies: ["SeaClawProtocol"],
            path: "Tests/SeaClawProtocolTests"
        ),
        .testTarget(
            name: "SeaClawClientTests",
            dependencies: ["SeaClawClient"],
            path: "Tests/SeaClawClientTests"
        ),
    ]
)
