import SwiftUI
import WidgetKit
import HumanChatUI

struct HumanStatusEntry: TimelineEntry {
    let date: Date
    let isConnected: Bool
    let statusText: String
}

struct HumanStatusProvider: TimelineProvider {
    func placeholder(in context: Context) -> HumanStatusEntry {
        HumanStatusEntry(date: Date(), isConnected: true, statusText: "Connected")
    }

    func getSnapshot(in context: Context, completion: @escaping (HumanStatusEntry) -> Void) {
        let entry = HumanStatusEntry(date: Date(), isConnected: true, statusText: "Connected")
        completion(entry)
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<HumanStatusEntry>) -> Void) {
        let entry = HumanStatusEntry(date: Date(), isConnected: true, statusText: "Connected")
        let nextUpdate = Calendar.current.date(byAdding: .minute, value: 15, to: Date()) ?? Date()
        let timeline = Timeline(entries: [entry], policy: .after(nextUpdate))
        completion(timeline)
    }
}

struct HumanWidgetEntryView: View {
    var entry: HumanStatusEntry

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                Circle()
                    .fill(entry.isConnected ? HUTokens.Dark.accent : HUTokens.Dark.textMuted)
                    .frame(width: 8, height: 8)
                Text("h-uman")
                    .font(.custom("Avenir-Heavy", size: 14))
                    .foregroundStyle(.primary)
            }
            Text(entry.statusText)
                .font(.custom("Avenir-Book", size: 12))
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
        .padding()
    }
}

struct HumanWidget: Widget {
    let kind: String = "HumanStatusWidget"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: HumanStatusProvider()) { entry in
            HumanWidgetEntryView(entry: entry)
        }
        .configurationDisplayName("h-uman Status")
        .description("Shows your assistant connection status")
        .supportedFamilies([.systemSmall, .systemMedium])
    }
}

#Preview(as: .systemSmall) {
    HumanWidget()
} timeline: {
    HumanStatusEntry(date: Date(), isConnected: true, statusText: "Connected")
}
