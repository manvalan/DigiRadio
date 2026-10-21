import Foundation

enum DigiRadioFriendlyError {
    static func message(for error: Error) -> String {
        guard let digi = error as? DigiRadioError else {
            return error.localizedDescription
        }
        switch digi {
        case .notConnected:
            return "Non connesso a DigiRadio."
        case .invalidHost:
            return "Indirizzo dispositivo non valido."
        case .decodingFailed:
            return "Risposta non valida dal dispositivo."
        case let .httpStatus(code, reason):
            return httpMessage(code: code, reason: reason)
        case let .network(underlying):
            let text = underlying.localizedDescription
            if text.localizedCaseInsensitiveContains("timed out") {
                return "Timeout — il dispositivo impiega più tempo del previsto. Riprova."
            }
            return text
        }
    }

    private static func httpMessage(code: Int, reason: String?) -> String {
        let body = reason ?? ""
        switch code {
        case 503:
            if body.contains("tuner") || body.contains("Tuner") {
                return "Tuner non disponibile — attendi l'avvio del modulo radio."
            }
            if body.contains("audio") || body.isEmpty {
                return "Servizio audio non disponibile — attendi l'avvio del DSP e riprova."
            }
            if body.contains("bluetooth") || body.contains("Bluetooth") {
                return "Modulo audio BT1035 non disponibile."
            }
            return "Servizio non disponibile sul dispositivo."
        case 500:
            if body.contains("store_failed") {
                return "Impossibile salvare sul dispositivo. Riprova."
            }
            if body.contains("not_booted") {
                return "Modulo audio spento. Accendilo da Impostazioni → Altoparlante."
            }
            if body.contains("at_timeout") {
                return "Il modulo audio non risponde. Tieni l’altoparlante in pairing e riprova."
            }
            return "Errore interno del dispositivo."
        case 400:
            if body.contains("invalid_mac") { return "Indirizzo MAC non valido." }
            if body.contains("not_booted") { return "Modulo audio spento. Accendilo da Impostazioni → Altoparlante." }
            if body.contains("at_timeout") { return "Il modulo audio non risponde. Tieni l’altoparlante in pairing e riprova." }
            if body.contains("invalid_ssid") { return "Nome rete Wi‑Fi non valido." }
            if body.contains("invalid_password") { return "Password Wi‑Fi non valida (8–63 caratteri per WPA)." }
            if body.contains("invalid_json") || body.contains("missing_field") {
                return "Richiesta non valida — verifica connessione e riprova."
            }
            if body.contains("missing_ensemble") {
                return "Preset DAB incompleto — riscansiona e risalva la stazione."
            }
            if body.contains("missing_frequency") {
                return "Preset FM incompleto — riscansiona e risalva la stazione."
            }
            if body.contains("invalid_name") {
                return "Nome stazione non valido."
            }
            return "Richiesta non accettata dal dispositivo."
        case 409:
            if body.contains("service_list_empty") {
                return "Elenco servizi DAB non ancora pronto — attendi un attimo e riprova."
            }
            if body.contains("wrong_band") {
                return "Il tuner non è in modalità DAB. Sintonizza prima un ensemble."
            }
            if body.contains("not_booted") {
                return "Modulo radio non pronto — attendi l'avvio del dispositivo."
            }
            if body.contains("hardware_failed") || body.contains("tune_failed") {
                return "Operazione tuner fallita — riprova o cambia canale."
            }
            if body.contains("service") || body.contains("dab") || body.contains("ensemble") {
                return "Impossibile leggere i servizi DAB — sintonizza prima un ensemble."
            }
            if body.contains("dab_service_not_found") {
                return "Servizio DAB non disponibile su questo ensemble — riscansiona o prova un'altra stazione."
            }
            if body.contains("\"full\"") || body.contains("full") && body.contains("station") {
                return "Elenco preset pieno (massimo 20 sul dispositivo)."
            }
            if body.contains("duplicate") {
                return "Stazione già presente nei preset."
            }
            if body.contains("tune") || body.contains("seek") || body.contains("scan") {
                return "Operazione tuner fallita — riprova o cambia banda."
            }
            return "Conflitto: un'altra sorgente audio è attiva o l'hardware non è pronto."
        default:
            return "Errore HTTP \(code)."
        }
    }
}
