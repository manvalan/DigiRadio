#!/usr/bin/env python3
"""Build the low-cost BOM of the DigiRadio v2 board from the KiCad schematic.

The original schematic and BOM are never changed. For each BOM row the script
reads live LCSC prices and stock for the parts approved in equivalents.json,
adds the Mouser prices stored in that file, and picks the part with the lowest
real purchase cost, including minimum order quantities.

    python3 build_bom.py --boards 5
    python3 build_bom.py --cerca        # also suggest cheaper C and R on LCSC

Output in output/: the full BOM (semicolon CSV for Excel) and one order file
per supplier. Rows missing from equivalents.json are marked DA MAPPARE and make
the script exit with status 1.
"""
import argparse
import csv
import json
import math
import re
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
KICAD_DIR = HERE.parent / "kicad"
SCHEMATIC = KICAD_DIR / "digiradio-batch2.kicad_sch"
PCB = KICAD_DIR / "digiradio-batch2.kicad_pcb"
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
USER_AGENT = "digiradio-bom/1.0"
LCSC_DETAIL = "https://wmsc.lcsc.com/ftps/wm/product/detail?productCode="
SEARCH_API = "https://jlcsearch.tscircuit.com/"
ECB_RATES = "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"
FALLBACK_USD_EUR = 0.92
MOUSER_MAX_AGE_DAYS = 30


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def usd_to_eur():
    try:
        root = ET.fromstring(fetch(ECB_RATES))
        rate = next(e.get("rate") for e in root.iter() if e.get("currency") == "USD")
        return 1 / float(rate), "BCE"
    except Exception as error:
        print(f"Cambio BCE non disponibile ({error}), uso {FALLBACK_USD_EUR}", file=sys.stderr)
        return FALLBACK_USD_EUR, "valore di riserva"


def export_bom():
    """Export the BOM from the schematic with kicad-cli, one row per PNM."""
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "bom.csv"
        subprocess.run(
            [KICAD_CLI, "sch", "export", "bom", str(SCHEMATIC), "-o", str(out),
             "--fields", "Reference,${QUANTITY},Value,PNM,Footprint,${DNP}",
             "--labels", "Reference,Qty,Value,PNM,Footprint,DNP",
             "--group-by", "Value,PNM,Footprint"],
            check=True, capture_output=True)
        return [r for r in csv.DictReader(out.open(encoding="utf-8")) if not r["DNP"]]


def expand_refs(refs):
    result = []
    for part in refs.split(","):
        match = re.fullmatch(r"([A-Za-z]+)(\d+)-[A-Za-z]*(\d+)", part)
        if match:
            result += [f"{match[1]}{i}" for i in range(int(match[2]), int(match[3]) + 1)]
        else:
            result.append(part)
    return result


def pcb_differences(rows):
    """Compare value and footprint of every schematic part with the PCB."""
    text = PCB.read_text(encoding="utf-8")
    found = re.findall(
        r'\(footprint "([^"]+)".*?\(property "Reference" "([^"]+)".*?\(property "Value" "([^"]+)"',
        text, re.S)
    pcb = {ref: (footprint, value) for footprint, ref, value in found}
    differences = []
    for row in rows:
        for ref in expand_refs(row["Reference"]):
            if ref not in pcb:
                differences.append(f"{ref} manca sul PCB")
            elif pcb[ref][0] != row["Footprint"]:
                differences.append(f"{ref} impronta: schema {row['Footprint']}, PCB {pcb[ref][0]}")
            elif pcb[ref][1] != row["Value"] and not row["Value"].startswith(pcb[ref][1]):
                differences.append(f"{ref} valore: schema {row['Value']}, PCB {pcb[ref][1]}")
    return differences


def lcsc_part(code, cache):
    """Return the live LCSC data of a part, or None if it cannot be read."""
    if code in cache:
        return cache[code]
    part = None
    try:
        result = json.loads(fetch(LCSC_DETAIL + code))["result"]
        if result:
            ladder = []
            for step in result.get("productPriceList") or []:
                discount = float(step.get("discountRate") or 1)
                ladder.append((step["ladder"], float(step["usdPrice"]) * discount))
            part = {
                "supplier": "LCSC", "code": code, "mpn": result["productModel"],
                "brand": result["brandNameEn"], "package": result.get("encapStandard") or "",
                "stock": result.get("stockNumber") or 0, "moq": result.get("minBuyNumber") or 1,
                "step": result.get("split") or 1, "ladder": ladder, "currency": "USD",
                "eta": "",
            }
    except Exception as error:
        print(f"LCSC {code}: {error}", file=sys.stderr)
    cache[code] = part
    time.sleep(0.2)
    return part


def mouser_part(entry):
    return {
        "supplier": "Mouser", "code": entry["mouser"], "mpn": entry["mpn"], "brand": entry["brand"],
        "package": "", "stock": entry["stock"], "moq": 1, "step": 1,
        "ladder": [tuple(step) for step in entry["eur"]], "currency": "EUR",
        "eta": entry.get("eta", ""),
    }


def resolve(entry, cache):
    return lcsc_part(entry["lcsc"], cache) if "lcsc" in entry else mouser_part(entry)


def purchase(part, need, usd_eur):
    """Return (pieces to buy, unit price EUR, total EUR), or None if there is no price."""
    if not part or not part["ladder"]:
        return None
    pieces = max(need, part["moq"])
    pieces = math.ceil(pieces / part["step"]) * part["step"]
    unit = None
    for quantity, price in sorted(part["ladder"]):
        if quantity <= pieces:
            unit = price
    if unit is None:
        pieces, unit = sorted(part["ladder"])[0]
    if part["currency"] == "USD":
        unit *= usd_eur
    return pieces, unit, pieces * unit


def best_offer(offers, need):
    """Pick the cheapest stocked part, then a part with a known date, then any part."""
    in_stock = [o for o in offers if o[0]["stock"] >= need]
    expected = [o for o in offers if o[0]["eta"]]
    return min(in_stock or expected or offers, key=lambda o: o[1][2], default=None)


def get_offers(candidates, need, usd_eur, cache):
    offers = []
    for entry in candidates:
        part = resolve(entry, cache)
        cost = purchase(part, need, usd_eur)
        if cost:
            offers.append((part, cost, entry))
    return offers


def euro(value):
    return "" if value is None else f"{value:.4f}".replace(".", ",")


def suggest(rows, parts):
    """Print cheaper LCSC capacitors and resistors that are not yet approved.

    These are leads with catalogue prices, not approved parts: check the voltage
    and dielectric, then add the part to equivalents.json.
    """
    for row in rows:
        kind = {"C": "capacitors", "R": "resistors"}.get(row["Reference"][0])
        package = re.search(r"_(\d{4})_", row["Footprint"])
        if not kind or not package:
            continue
        value = parse_value(row["Value"], kind)
        if value is None:
            continue
        key = "capacitance" if kind == "capacitors" else "resistance"
        query = urllib.parse.urlencode({"package": package[1], key: value})
        listed = json.loads(fetch(f"{SEARCH_API}{kind}/list.json?{query}"))[kind]
        known = {e.get("lcsc") for e in parts.get(row["PNM"].strip(), {}).get("candidates", [])}
        cheap = sorted((c for c in listed if c.get("price1")), key=lambda c: c["price1"])
        lines = [f"C{c['lcsc']} {c['mfr']} {c.get('voltage_rating') or ''}"
                 f"{'V ' + c['temperature_coefficient'] if c.get('temperature_coefficient') else ''}"
                 f" ${c['price1']}" for c in cheap if f"C{c['lcsc']}" not in known][:3]
        print(f"{row['Reference'][:24]:24} {row['Value']:8} {package[1]}: " + "; ".join(lines))


def parse_value(text, kind):
    factors = {"p": 1e-12, "n": 1e-9, "u": 1e-6} if kind == "capacitors" else {"k": 1e3, "K": 1e3, "": 1}
    match = re.fullmatch(r"([\d.]+)([pnuKk]?)F?", text)
    return float(match[1]) * factors[match[2]] if match and match[2] in factors else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--boards", type=int, default=1, help="numero di schede (default 1)")
    parser.add_argument("--spare", type=int, default=0, help="pezzi di scorta per ogni riga")
    parser.add_argument("--usd-eur", type=float, help="cambio USD-EUR, altrimenti quello BCE del giorno")
    parser.add_argument("--cerca", action="store_true", help="suggerisce C e R più economici su LCSC")
    parser.add_argument("--out", type=Path, default=HERE / "output")
    args = parser.parse_args()

    data = json.loads((HERE / "equivalents.json").read_text(encoding="utf-8"))
    parts = data["parts"]
    rows = export_bom()
    if args.cerca:
        suggest(rows, parts)
        return 0

    usd_eur, rate_source = (args.usd_eur, "--usd-eur") if args.usd_eur else usd_to_eur()
    cache, output, orders = {}, [], {"LCSC": [], "Mouser": []}
    total, total_original, unmapped, attention = 0.0, 0.0, [], []

    for row in rows:
        pnm = row["PNM"].strip()
        need = int(row["Qty"]) * args.boards + args.spare
        entry = parts.get(pnm)
        line = {"Riferimenti": row["Reference"], "Valore": row["Value"], "Qta scheda": row["Qty"],
                "Qta necessaria": need, "MPN schema": pnm, "Impronta": row["Footprint"]}
        if not entry:
            unmapped.append(pnm)
            line["Stato"] = "DA MAPPARE"
            output.append(line)
            continue
        options = "; ".join(
            f"{o['lcsc']}: {o['note']}" for o in entry.get("options", []))
        line.update({"Note": entry.get("note", ""), "Opzioni da verificare": options})
        if entry.get("external"):
            line.update({"Stato": "ALTRO FORNITORE", "Fornitore": entry["external"]})
            attention.append(f"{row['Reference']}: {entry['note']}")
            output.append(line)
            continue

        offers = get_offers(entry["candidates"], need, usd_eur, cache)
        best = best_offer(offers, need)
        if not best:
            line["Stato"] = "NESSUN PREZZO"
            attention.append(f"{row['Reference']} {pnm}: nessun prezzo disponibile")
            output.append(line)
            continue

        part, (pieces, unit, cost), chosen = best
        status = "Originale" if chosen.get("orig") else "Sostituito"
        if part["stock"] < need:
            status += ", NON DISPONIBILE"
            detail = part["eta"] or f"stock {part['stock']}"
            attention.append(f"{row['Reference']} {part['mpn']}: stock insufficiente ({detail})")
        if entry.get("note", "").startswith("DA VERIFICARE"):
            attention.append(f"{row['Reference']} {part['mpn']}: {entry['note']}")
        line.update({
            "Stato": status, "Fornitore": part["supplier"], "Codice": part["code"],
            "MPN scelto": part["mpn"], "Marca": part["brand"], "Package": part["package"],
            "Stock": part["stock"], "Pezzi da comprare": pieces,
            "Prezzo unit EUR": euro(unit), "Costo EUR": euro(cost), "Consegna": part["eta"],
        })
        total += cost
        orders[part["supplier"]].append((part["code"], part["mpn"], pieces, row["Reference"]))

        original = best_offer([o for o in offers if o[2].get("orig")], need)
        original_cost = original[1][2] if original else cost
        line["Costo originale EUR"] = euro(original_cost)
        total_original += original_cost
        output.append(line)

    args.out.mkdir(parents=True, exist_ok=True)
    suffix = f"{args.boards}-schede"
    columns = ["Riferimenti", "Valore", "Qta scheda", "Qta necessaria", "Stato", "Fornitore", "Codice",
               "MPN scelto", "Marca", "Package", "Stock", "Pezzi da comprare", "Prezzo unit EUR",
               "Costo EUR", "Costo originale EUR", "Consegna", "MPN schema", "Impronta", "Note",
               "Opzioni da verificare"]
    bom_path = args.out / f"digiradio-batch2-bom-economica-{suffix}.csv"
    with bom_path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, columns, delimiter=";")
        writer.writeheader()
        writer.writerows(output)
        writer.writerow({"Riferimenti": "TOTALE, BT1058 e spedizione esclusi",
                         "Costo EUR": euro(total), "Costo originale EUR": euro(total_original)})

    for supplier, lines in orders.items():
        with (args.out / f"ordine-{supplier.lower()}-{suffix}.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["Part Number", "MPN", "Quantity", "Customer Reference"])
            for code, mpn, pieces, reference in lines:
                writer.writerow([code, mpn, pieces, reference[:60]])

    by_supplier = {name: sum(1 for line in output if line.get("Fornitore") == name) for name in orders}
    spend = {name: sum(float(line["Costo EUR"].replace(",", ".")) for line in output
                       if line.get("Fornitore") == name and line.get("Costo EUR")) for name in orders}
    print(f"{args.boards} schede, 1 USD = {usd_eur:.4f} EUR ({rate_source})")
    print(f"Totale {total:.2f} EUR contro {total_original:.2f} EUR con i soli originali")
    for name in orders:
        print(f"  {name}: {by_supplier[name]} righe, {spend[name]:.2f} EUR")
    age = (date.today() - date.fromisoformat(data["mouser_checked"])).days
    if age > MOUSER_MAX_AGE_DAYS:
        print(f"ATTENZIONE: i prezzi Mouser hanno {age} giorni, aggiornali in equivalents.json")
    for text in attention:
        print(f"ATTENZIONE: {text}")
    for text in pcb_differences(rows):
        print(f"SCHEMA/PCB: {text}")
    print(f"File: {bom_path}")
    if unmapped:
        print("DA MAPPARE in equivalents.json: " + ", ".join(unmapped))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
