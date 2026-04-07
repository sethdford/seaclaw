// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "human-ondevice",
    platforms: [.macOS(.v14)],
    dependencies: [
        .package(path: "../../shared/HumanKit"),
    ],
    targets: [
        .executableTarget(
            name: "human-ondevice",
            dependencies: [
                .product(name: "HumanOnDeviceServer", package: "HumanKit"),
            ],
            path: "Sources"
        ),
    ]
)
