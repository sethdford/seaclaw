import Foundation
import HumanOnDeviceServer

let args = CommandLine.arguments
var port: UInt16 = 11435

for (i, arg) in args.enumerated() {
    if (arg == "--port" || arg == "-p"), i + 1 < args.count,
       let p = UInt16(args[i + 1]) {
        port = p
    }
    if arg == "--help" || arg == "-h" {
        print("""
        human-ondevice — on-device AI inference server
        
        Serves an OpenAI-compatible API backed by Apple Intelligence.
        Zero dependencies, zero cost, fully private.
        
        Usage: human-ondevice [--port PORT]
        
        Options:
          --port, -p    Port to listen on (default: 11435)
          --help, -h    Show this help message
        
        Endpoints (require `Authorization: Bearer <token>`; token is printed on startup):
          GET  /v1/models              List available models
          POST /v1/chat/completions    Chat completion (streaming supported)
          GET  /health                 Health check
        """)
        exit(0)
    }
}

if #available(macOS 26.0, *) {
    let server = OnDeviceServer(port: port)
    do {
        try server.start()
        print("[human-ondevice] listening on http://127.0.0.1:\(port)/v1")
        print("[human-ondevice] model: apple-foundationmodel (context: 4096 tokens)")
        dispatchMain()
    } catch {
        fputs("[human-ondevice] failed to start: \(error)\n", stderr)
        exit(1)
    }
} else {
    fputs("[human-ondevice] requires macOS 26.0 or later with Apple Intelligence\n", stderr)
    exit(1)
}
