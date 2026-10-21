import SwiftUI

struct FMRadioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var homeViewModel: HomeViewModel?

    var body: some View {
        PresetsView(lockedBand: .fm)
            .onAppear {
                if homeViewModel == nil {
                    homeViewModel = HomeViewModel(service: environment.digiRadio)
                }
                homeViewModel?.onAppear()
                environment.stationScan.selectedBand = .fm
                Task {
                    await environment.adauStudio.applyActiveSource(.radio)
                    await homeViewModel?.selectFM()
                    await environment.stationScan.refreshLibrary()
                }
            }
            .onDisappear {
                homeViewModel?.onDisappear()
            }
    }
}
