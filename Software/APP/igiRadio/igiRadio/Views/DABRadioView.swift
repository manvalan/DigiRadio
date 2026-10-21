import SwiftUI

struct DABRadioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var homeViewModel: HomeViewModel?

    var body: some View {
        PresetsView(lockedBand: .dab)
            .onAppear {
                if homeViewModel == nil {
                    homeViewModel = HomeViewModel(service: environment.digiRadio)
                }
                homeViewModel?.onAppear()
                environment.stationScan.selectedBand = .dab
                Task {
                    await environment.adauStudio.applyActiveSource(.radio)
                    await homeViewModel?.selectDAB()
                    await environment.stationScan.refreshLibrary()
                }
            }
            .onDisappear {
                homeViewModel?.onDisappear()
            }
    }
}
