# D1-lite-Light-Header

Controller für zwei adressierbare RGB-LED-Strips, eine einzelne PWM-LED, zwei Relais und mehrere Sensoren auf Basis eines ESP8266.

Dieses Projekt stellt eine strukturierte JSON-API über die serielle Schnittstelle bereit. Zusätzlich gibt es mit `api_tester.py` eine Weboberfläche, mit der sich die Funktionen bequem im Browser prüfen lassen.

## Schnellstart

1. Repository von GitHub klonen oder den lokalen Projektordner in VS Code öffnen.
2. Python 3 unter Windows installieren.
3. VS Code und die PlatformIO Extension einrichten.
4. Die Firmware mit PlatformIO bauen und auf das Board laden.
5. Den Python-API-Tester starten und die Weboberfläche im Browser aufrufen.

---

## 1. Python 3 unter Windows installieren

Der API-Tester benötigt Python 3. Installieren Sie Python aus einer verlässlichen Quelle, am besten direkt von python.org.

### Empfohlene Installation

1. Öffnen Sie die offizielle Download-Seite: https://www.python.org/downloads/windows/
2. Laden Sie die aktuelle Python-3-Version für Windows herunter.
3. Starten Sie das Installationsprogramm.
4. Aktivieren Sie am ersten Installationsbildschirm unbedingt die Option `Add python.exe to PATH`.
5. Wählen Sie danach `Install Now` oder eine vergleichbare Standardinstallation.

### Prüfen der Installation

öffnen Sie eine Eingabeaufforderung oder PowerShell und führen Sie aus:

```powershell
py --version
pip --version
```

Wenn beide Befehle eine Versionsnummer anzeigen, ist Python korrekt eingerichtet.

### Falls `python` nicht gefunden wird

Wenn Windows `python` nicht erkennt, prüfen Sie:

1. Ob Python wirklich installiert wurde.
2. Ob `Add python.exe to PATH` bei der Installation aktiv war.
3. Ob Sie die Konsole nach der Installation neu geöffnet haben.

---

## 2. Projekt von GitHub herunterladen und in VS Code öffnen

### Variante A: Mit Git klonen

1. Öffnen Sie ein Terminal in dem Ordner, in dem das Projekt liegen soll.
2. Klonen Sie das Repository:

```powershell
git clone git@github.com:Wiroka-projects/D1-lite-Light-Header.git
```

3. Wechseln Sie in den Projektordner:

```powershell
cd "LED Transistor"
```

4. öffnen Sie den Ordner in VS Code:

```powershell
code .
```

### Variante B: Ohne Git

Wenn Sie das Repository bereits als ZIP heruntergeladen haben, entpacken Sie es an einen beliebigen Ort und öffnen Sie anschliessend den Projektordner direkt in VS Code.

---

## 3. VS Code und PlatformIO einrichten

### VS Code installieren

1. Laden Sie Visual Studio Code von https://code.visualstudio.com/ herunter.
2. Installieren Sie VS Code mit den Standardoptionen.
3. Starten Sie VS Code einmal nach der Installation.

### PlatformIO Extension installieren

1. Öffnen Sie in VS Code die Erweiterungen über das Symbol in der Seitenleiste oder mit `Strg+Shift+X`.
2. Suchen Sie nach `PlatformIO IDE`.
3. Installieren Sie die Erweiterung von PlatformIO.
4. Starten Sie VS Code neu, falls Sie dazu aufgefordert werden.

### Projekt mit PlatformIO öffnen

1. Öffnen Sie den Ordner `LED Transistor` in VS Code.
2. Warten Sie, bis PlatformIO das Projekt erkannt hat.
3. Prüfen Sie, ob die PlatformIO-Umgebung geladen wurde und keine fehlenden Abhängigkeiten gemeldet werden.

---

## 4. Firmware bauen und auf das Board laden

1. Verbinden Sie den ESP8266 per USB mit dem Rechner.
2. Öffnen Sie das PlatformIO-Menue in VS Code.
3. Wählen Sie `Build`, um die Firmware zu kompilieren.
4. Wählen Sie `Upload`, um die Firmware auf das Board zu flashen.
5. Öffnen Sie bei Bedarf den seriellen Monitor, um die Ausgabe der Firmware zu sehen.

Wenn der Upload nicht startet, prüfen Sie den richtigen COM-Port und ob das Board zur gewählten Plattform passt.

---

## 5. Python-API-Tester ausführen

Die Datei `api_tester.py` startet eine FastAPI-Weboberfläche, über die Sie Befehle an den Controller senden können.

### Virtuelle Umgebung anlegen

Empfohlen ist eine lokale Python-Umgebung im Projektordner. So bleiben Abhängigkeiten getrennt vom restlichen System.

1. Öffnen Sie PowerShell im Projektordner.
2. Erstellen Sie die virtuelle Umgebung:

```powershell
py -m venv .venv
```

3. Aktivieren Sie die Umgebung:

```powershell
.\.venv\Scripts\Activate.ps1
```

### Abhängigkeiten installieren

Im Projekt liegt eine `requirements.txt` mit den benötigten Paketen. Installieren Sie diese im Projektordner:

```powershell
python -m pip install -r requirements.txt
```

Alternativ können Sie die Pakete auch einzeln installieren:

```powershell
python -m pip install fastapi uvicorn pyserial
```

### Seriellen Port bestimmen

Der Tester kommuniziert mit dem ESP8266 über einen COM-Port. Wenn kein Port per Parameter angegeben wird, zeigt das Skript die verfügbaren Ports an und fragt interaktiv nach dem richtigen Anschluss.

Optional können Sie den Port direkt übergeben:

```powershell
python api_tester.py --port COM10
```

### Tester starten

```powershell
python api_tester.py
```

Beim Start zeigt das Skript die Webadresse an. Laut aktueller Konfiguration läuft die Oberfläche auf:

```text
http://localhost:8090
```

Zusätzlich stellt FastAPI die automatische API-Dokumentation bereit:

```text
http://localhost:8090/docs
```

### Aufruf im Browser

1. Starten Sie den Python-Prozess.
2. Öffnen Sie im Browser `http://localhost:8090`.
3. Nutzen Sie die Weboberfläche, um RGB-Strips, LED, Relais, Sensoren und Konfigurationen zu testen.

---

## 6. Was in der Firmware möglich ist

Die Firmware versteht JSON-Kommandos über die serielle Schnittstelle. Die wichtigsten Befehlsgruppen sind:

- `rgb` für einzelne Pixel, Bereiche, ganze Strips und Animationen.
- `led` für die einzelne PWM-LED.
- `relay` für beide Relais.
- `read` für Sensorabfragen.
- `config` für permanente Einstellungen.
- `status` für den aktuellen Status.

Geben Sie im seriellen Monitor `help` ein, um die Help-Ausgabe direkt vom Controller zu erhalten.

### RGB-Strip-Steuerung

```json
{"action":"rgb","strip":1,"mode":"single","pixel":0,"r":255,"g":0,"b":0,"brightness":128}
{"action":"rgb","strip":1,"mode":"range","start":0,"end":9,"r":0,"g":255,"b":0,"brightness":200}
{"action":"rgb","strip":2,"mode":"all","r":0,"g":0,"b":255,"brightness":255}
{"action":"rgb","strip":1,"mode":"clear"}
{"action":"rgb","strip":1,"mode":"running","r":255,"g":120,"b":40,"brightness":180,"count":12,"time":40,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"charging","r":0,"g":180,"b":255,"brightness":200,"time":35,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"center_fill","r":0,"g":255,"b":120,"brightness":180,"time":35,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"rainbow","brightness":180,"time":20,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"flash","r":255,"g":255,"b":255,"brightness":255,"time":250,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"random","brightness":255,"time":80,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"breathing","r":255,"g":120,"b":40,"brightness":180,"time":25,"repeatingtime":0}
{"action":"rgb","strip":1,"mode":"default"}
```

### LED-Steuerung

```json
{"action":"led","mode":"digital","state":true}
{"action":"led","mode":"analog","value":128}
```

### Relais-Steuerung

```json
{"action":"relay","relay":1,"state":true}
{"action":"relay","relay":2,"state":false}
```

### Sensoren lesen

```json
{"action":"read","sensor":"lb","mode":"analog"}
{"action":"read","sensor":"lb","mode":"digital"}
{"action":"read","sensor":"rs"}
{"action":"read","sensor":"temp"}
```

### Konfiguration

```json
{"action":"config","setting":"lb_threshold","value":600}
{"action":"config","setting":"rgb1_pixels","value":100}
{"action":"config","setting":"rgb2_pixels","value":100}
{"action":"config","setting":"rgb1_default_color","r":255,"g":255,"b":255}
{"action":"config","setting":"rgb2_default_color","r":255,"g":255,"b":255}
{"action":"config","setting":"rgb1_brightness","value":128}
{"action":"config","setting":"rgb2_brightness","value":200}
{"action":"config","setting":"led_default","value":128}
{"action":"config","setting":"running_default","strip":1,"r":255,"g":120,"b":40,"brightness":180,"time":40,"repeatingtime":0,"count":12}
{"action":"config","setting":"charging_default","strip":1,"r":0,"g":180,"b":255,"brightness":200,"time":35,"repeatingtime":0}
{"action":"config","setting":"center_default","strip":1,"r":0,"g":255,"b":120,"brightness":180,"time":35,"repeatingtime":0}
{"action":"config","setting":"rainbow_default","strip":1,"brightness":180,"time":20,"repeatingtime":0}
{"action":"config","setting":"flash_default","strip":1,"r":255,"g":255,"b":255,"brightness":255,"time":250,"repeatingtime":0}
{"action":"config","setting":"random_default","strip":1,"brightness":255,"time":80,"repeatingtime":0}
{"action":"config","setting":"breathing_default","strip":1,"r":255,"g":120,"b":40,"brightness":180,"time":25,"repeatingtime":0}
{"action":"config","setting":"startup_mode","strip":1,"value":"effect"}
{"action":"config","setting":"startup_effect","strip":1,"value":"rainbow"}
```

### Status-Abfrage

```json
{"action":"status"}
```

---

## 7. Parameter-Referenz

- `strip`: 1 für Ring-Top oder 2 für Door.
- `pixel`: Pixelindex innerhalb des jeweiligen Strips.
- `r`, `g`, `b`: RGB-Farbwerte von 0 bis 255.
- `brightness`: Helligkeit von 0 bis 255.
- `count`: Länge eines Running-Light-Effekts.
- `time`: Intervall zwischen Animationsschritten in Millisekunden.
- `repeatingtime`: Wiederholzähler, `0` bedeutet endlos bis zum nächsten Befehl.
- `value`: Dimmwert für die einzelne LED.
- `state`: `true` oder `false`.
- `lb_threshold`: Schwellwert für den LB-Sensor im Digitalmodus.
- `rgb1_pixels` und `rgb2_pixels`: Anzahl der LEDs pro Strip.
- `rgb1_default_color` und `rgb2_default_color`: Standardfarbe beim Start.
- `rgb1_brightness` und `rgb2_brightness`: Standardhelligkeit für RGB-Befehle.
- `led_default`: Standardwert für die einzelne LED beim Start.
- `startup_mode`: `solid` oder `effect`.
- `startup_effect`: `running`, `charging`, `center_fill`, `rainbow`, `flash`, `random` oder `breathing`.

---

## 8. Hinweise zur Fehlersuche

Wenn etwas nicht auf Anhieb funktioniert, prüfen Sie diese Punkte:

1. Ist das richtige Board in PlatformIO ausgewählt?
2. Ist der korrekte COM-Port für den ESP8266 verwendet worden?
3. Ist die Firmware erfolgreich auf das Board geladen worden?
4. Ist der serielle Monitor auf `115200` Baud eingestellt?
5. Wurde der Python-Tester mit dem richtigen Port gestartet?

Wenn die Weboberfläche startet, aber keine Rückmeldung vom Controller kommt, liegt die Ursache in der Regel an der seriellen Verbindung oder am falschen Port.

---

## 9. Projektstruktur

```text
api_tester.py    - FastAPI-Weboberfläche für Tests
auto_upload.py   - Hilfsskript für Upload-Automatisierung
platformio.ini   - PlatformIO-Konfiguration
src/main.cpp     - ESP8266-Firmware mit JSON-API
requirements.txt - Python-Abhängigkeiten für den Tester
```

