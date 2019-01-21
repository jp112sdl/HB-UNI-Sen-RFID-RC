# HB-UNI-Sen-RFID-RC

_work in progress..._

Dieses Projekt soll in Anlehnung an mein [Wemos-RFID-Alarmanlagen-Projekt](https://github.com/jp112sdl/WemosD1_HomeMatic_RFID) eine netzwerkinfrastrukturunabhängige Lösung bieten.

#### Bauteile
- 1x Arduino Pro Mini (3.3V / 8MHz)
- 1x CC1101 868MHz Funkmodul
- 1x MFRC522 RFID Reader 
- 3x Widerstand 330 Ohm
- 1x Duo-LED
- 1x LED 
- 1x Taster
- 1x Buzzer aktiv (z.B. [eBay](https://www.ebay.de/itm/332081758028))

#### Prototyp / Versuchsaufbau:<br/>
<img src="Images/Proto1.jpeg" width=400></img><br/><img src="Images/Proto2.jpeg" width=400></img>

#### Verdrahtung
![wiring](Images/wiring.png)

#### CCU Geräte/Bedienung
![ccu_einstellungen](Images/CCU_Geraete.png)

#### CCU Einstellungen
![ccu_bedienung](Images/CCU_Einstellungen.png)

#### Steuerbefehle

Mithilfe von SUBMIT-Commands lassen sich einige Funktionen des Device steuern.<br/>
Das Absetzen der Befehle erfolgt aus einem Skript heraus allgemein mit<br/>
`dom.GetObject("BidCos-RF.<Device Serial>:<Ch#>.SUBMIT").State("<command>");`

| Funktion | Command |
|----------|---------|
|Setzen einer Chip ID|`0x08,0x15,0xca,0xfe`|
|Löschen einer Chip ID|`0xcc`|
|Erzwingen der Chip ID Übertragung zur CCU|`0xfe`|
|Invertieren der StandbyLed|`0xff,0x01` (invertiert) oder `0xff,0x00` (normal)|
|Buzzer EIN (dauerhaft)|`0xb1`|
|Buzzer AUS|`0xb0`|
|Buzzer Beep Intervall endlos|`0xba`|

Beispiel - Löschen der RFID Chip ID Kanal 4:<br>
`dom.GetObject("BidCos-RF.JPRFID0001:4.SUBMIT").State("0xcc");`
