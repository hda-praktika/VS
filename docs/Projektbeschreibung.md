# Projektbeschreibung: Smart Energy
HB Solutions GbR: Damit Sie nicht gleich in die Luft gehen müssen.

## Einführung
In diesem Dokument werden die Anforderungen für das Projekt _Smart Energy_, in Auftrag gegeben durch die Rüdensche Windenergie AG (RWE) und durchgeführt von der HB Solutions GbR, aufgestellt.
_Smart Energy_ sieht vor, eine zentrale Kraftwerkssteuerung für Versorger und Verbraucher in einem Stromnetz zu erstellen.
Die Versorger reichen von einer Vielzahl an unterschiedlichen Kraftwerken wie etwa für Solar-, Wind- und Biomasseenergie.
Der Kunde RWE bedient sowohl Haushalte als auch Unternehmen als Verbraucher.
Stromspeicher sieht der Kunde nicht in der Kraftwerkssteuerung vor.
Die Kraftwerke werden zunächst simuliert, damit die Software zunächst vollständig fertiggestellt und getestet werden kann, bevor sie in einer Realumgebung eingesetzt wird.

## Anforderungsanalyse
Das Projekt setzt sich aus mehreren Komponenten zusammen.
Dabei steht im Zentrum die Kraftwerkssteuerung.
Weitere Komponenten sind Erzeuger und Verbraucher.
Ferner können weitere Komponenten wie Browser oder Energieversorger über definierte Schnittstellen mit dem System interagieren (z.B. historische Daten abfragen).

### Kraftwerkssteuerung
Die zentrale Kraftwerkssteuerung koordiniert Erzeuger und Verbraucher.
Zusätzlich soll die zentrale Kraftwerkssteuerung in der Lage sein, per TCP/HTTP mit jedem beliebigen Webbrowser zu kommunizieren.
Dafür stellt die Kraftwerkssteuerung einen HTTP-Server mit einer REST-API zur Verfügung, die mindestens `GET`-Anfragen unterstützt.
Über diese REST-API können dann externe Komponenten wie ein Webbrowser auf die Steuerung zugreifen.
Alternativ dazu, soll die Kraftwerkssteuerung selbst auch per RPC-Schnittstelle konfigurierbar sein, die externe Clients wie Energieversorger nutzen können.
Informationen der Erzeuger und Verbraucher werden sowohl vorerst per UDP, als auch später per MQTT empfangen. 

### Erzeuger / Verbraucher
Die Erzeuger und Verbraucher teilen ihre Informationen mit der Kraftwerkssteuerung vorerst per UDP, im späteren Verlauf des Projekts aber auch via MQTT.
Die Konfiguration der Komponenten durch die Kraftwerkssteuerung soll jedoch per RPC geschehen.
Dabei ist darauf zu achten, dass Versorger und Verbraucher klar identifizierbar sind, die Art des Teilnehmers bekannt ist, der Stromverbrauch/-erzeugung in kW angegeben ist und er per UDP mit der Zentralen Kraftwerkssteuerung kommuniziert. 
Der Kunde sieht für die Simulation mindestens vier Verbraucher/Erzeuger vor, darunter mindestens ein Verbraucher. 
Zum verbesserten Testen soll der Verbrauch bzw. die erzeugte Menge  künstlich variieren können.

### Nicht-funktionale Anforderungen
Das Projekt wird in C++ programmiert. Da die Vorgabe des Kunden ist, in C++ oder in Java zu programmieren.  

### Funktionale Anforderungen

1. Enegrieversorger sowie Verbraucher erstellen
   1. Verbraucher und Versoger müssen klar identifizierbar sein
      1. Art des Teilnehmners
      2. Eindeutige ID oder Name
      3. Aktuelle Stromenge in kW
         1. Soll variieren können
      4. Kommunikation über UDP
      5. Steuerung über RPC
   2. Es müssen mind 4. unabhängige Erzeuger/Verbraucher erstellt werden (davon mind. 1 Verbraucher)
2. Zentrale Kraftwerkssteuerung (ZK) erstellen
   1. Horizontal skalierbar
   2. UPD Schnitstelle für Verbraucher/Erzeuger
   3. Native HTTP GET Anfragen verarbeiten d.h. einen HTTP Server hosten
      1. Soll mit beliebigen Browsern arbeiten können
   4. TCP/HTTP für externe Webseite
   5. RPC Schnittstelle implementieren
3. Externen Client erstellen
   1. RPC Abfrage an ZK erstellen


### Nicht-Funktionale Anforderungen
1. Programmiersprache C++
2. Gitlab CI (Docker und Docker-compose)
3. Clean Code
4. Hygiene des Git-Repositories
5. Dokumentation
6. Lizenzen
7. `Dockerfile` und `docker-compose.yml` Beispiele

## Systemdesign

## Testing

## Projektplan

## Deployment
