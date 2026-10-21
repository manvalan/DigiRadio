import SwiftUI

struct AdauStudioView: View {
    @Bindable var vm: AdauStudioViewModel
    private let initialSection: AdauStudioSection?

    init(vm: AdauStudioViewModel, initialSection: AdauStudioSection? = nil) {
        self.vm = vm
        self.initialSection = initialSection
    }

    var body: some View {
        Group {
            if focus == .sound {
                SoundAtmosphereView(vm: vm)
            } else {
                AudioSettingsView(vm: vm, focus: focus)
            }
        }
        .onAppear {
            if let initialSection { vm.section = initialSection }
        }
    }

    private var focus: AudioSettingsFocus {
        switch initialSection ?? vm.section {
        case .listening: .listening
        case .sound: .sound
        case .diagnostics: .diagnostics
        }
    }
}
