// swift-tools-version: 5.9
import PackageDescription

// Note: This package provides the iOS app source as a library target.
// Build as an iOS app via Xcode: File > New Project > import this package,
// or use `xcodegen` with the project.yml spec to generate the .xcodeproj.

let package = Package(
    name: "HumaniOS",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "HumaniOS", targets: ["HumaniOS"]),
    ],
    dependencies: [
        .package(path: "../shared/HumanKit"),
    ],
    targets: [
        .target(
            name: "HumaniOS",
            dependencies: [
                .product(name: "HumanClient", package: "HumanKit"),
                .product(name: "HumanChatUI", package: "HumanKit"),
                .product(name: "HumanProtocol", package: "HumanKit"),
                .product(name: "HumanOnDevice", package: "HumanKit"),
            ],
            path: "Sources/HumaniOS",
            exclude: ["Resources/Info.plist"],
            resources: [
                .process("Resources"),
            ]
        ),
    ]
)
