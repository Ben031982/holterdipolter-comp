# Holterdipolter Comp – Build

## Auf GitHub bauen lassen (ohne lokale Installation)

1. Diesen Ordnerinhalt in ein GitHub-Repository legen (Branch `main`), inklusive `.github/`.
2. Jeder Push auf `main` startet den Build; manuell geht es unter
   Actions -> "Build VST3" -> "Run workflow".
3. Nach dem Lauf im Workflow-Run unter "Artifacts" `HolterdipolterComp-VST3-Windows` herunterladen,
   entpacken und den Ordner `Holterdipolter Comp.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.

JUCE 9.0.1 wird dabei automatisch geholt; es muss nicht im Repository liegen.

## Lokal bauen (Windows, Visual Studio, JUCE 9.0.1)

JUCE wird unter `C:\JUCE-9.0.1` gesucht (anderer Ort: `-DJUCE_DIR=<Pfad>`). Fehlt es dort, wird es automatisch von GitHub geholt.

In einer "x64 Native Tools"-Eingabeaufforderung bzw. PowerShell im Projektordner:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

Ergebnis:

    build\HolterdipolterComp_artefacts\Release\VST3\Holterdipolter Comp.vst3

Diesen Ordner nach `C:\Program Files\Common Files\VST3\` kopieren und in Ableton neu scannen.

## Trigger in Ableton

- Sync:  laeuft im Host-Tempo, kein Routing noetig.
- MIDI:  MIDI-Spur anlegen, "MIDI To" auf die Spur mit dem Plugin -> "Holterdipolter Comp". Jede Note startet die Kurve.
- Audio: im Plugin-Geraet oben die Sidechain-Quelle (z. B. Kick-Spur) waehlen, Threshold einstellen.
         Stellt der Host keinen Sidechain-Eingang bereit, triggert das eigene Eingangssignal.

## Kurven editieren

Die 16 Slots werden ueber die Miniaturen unter der grossen Kurve ausgewaehlt (keine Namen).
Jeder Slot ist frei editierbar und wird mit dem Projekt/Preset gespeichert.

- Punkt ziehen: verschieben (erster und letzter Punkt nur in der Hoehe)
- Doppelklick auf freie Flaeche: neuer Punkt
- Doppelklick auf Punkt: Punkt loeschen
- Linie zwischen zwei Punkten hoch/runter ziehen: Biegung des Segments
- Reset: aktueller Slot zurueck auf Werkskurve

## Wellenform in der Kurvenanzeige

Hinter der Kurve laeuft die Wellenform eines Durchlaufs mit:
blass = Original (Eingang), hell = Ergebnis nach dem Ducking.
Die Hoehe wird automatisch an den Pegel angepasst.
