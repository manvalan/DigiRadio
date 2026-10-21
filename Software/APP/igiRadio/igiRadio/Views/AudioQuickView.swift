import SwiftUI

struct AudioQuickView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        AudioSettingsView(vm: environment.adauStudio, focus: .listening)
    }
}

struct AudioView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        AudioSettingsView(vm: environment.adauStudio, focus: .listening)
    }
}
