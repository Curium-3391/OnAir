# ISM sensors: coverage of rtl_433's device protocols

The ISM sensors mode decodes devices the way rtl_433 does (https://github.com/merbanan/rtl_433, GPL-2.0-or-later; credits in THIRD_PARTY.md). Each decoder in `core/src/ism/` is written from the protocol description of one rtl_433 `src/devices/` file, which `ismProtocols()` names, and has a test vector built from that description in `tests/ism/`.

Compared against the rtl_433 README list of device protocols (380 entries; the ones marked * are disabled by default there because they are weak or ambiguous).

**Covered: 45 of 380 rtl_433 protocol numbers.** The rest is listed below by category. Key fobs and rolling-code remotes are deliberately not added.

Not an rtl_433 list entry but also decoded: Z-Wave R1 / R2 / R3, Sigfox (their own modes), and the FM/FSK test scenes.


## Car key fob / rolling-code remote (not added: security) (0 of 31 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 64 | Honda Car Key | not covered (disabled by default in rtl_433) |
| 93 | Ford Car Key | not covered (disabled by default in rtl_433) |
| 131 | Microchip HCS200/HCS300 KeeLoq Hopping Encoder based remotes | not covered |
| 164 | Security+ 2.0 (Keyfob) | not covered |
| 178 | Security+ (Keyfob) | not covered |
| 218 | Microchip HCS200/HCS300 KeeLoq Hopping Encoder based remotes (FSK) | not covered |
| 276 | RainPoint HCS012ARF Rain Gauge sensor | not covered |
| 301 | Astrostart 2000 Car Remote (-f 372.4M) | not covered |
| 302 | Compustar 1WG3R Car Remote | not covered |
| 303 | Chrysler Car Remote (-f 315.1M -s 920k) | not covered |
| 304 | Nidec Car Remote (-f 313.8M -s 1024k) | not covered (disabled by default in rtl_433) |
| 305 | Audiovox PRO-OE3B Car Remote (-f 303.4M) | not covered |
| 306 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (Sync, 833 bit/s) | not covered |
| 307 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (Sync, 1667 bit/s) | not covered |
| 308 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (No Sync, 833 bit/s) | not covered |
| 309 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (No Sync, 1667 bit/s) | not covered |
| 310 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (PIWM, 2500 bit/s) | not covered |
| 311 | Microchip HCS361 KeeLoq Hopping Encoder based remotes (-f 315.1M) (PIWM, 5000 bit/s) | not covered |
| 312 | MIC 6SC2 Car Remote (-f 315.1M) | not covered |
| 313 | GM ABO1502T Car Remote (-f 314.9M) | not covered |
| 314 | Siemens 5WY72XX Car Remote (-f 315.1M) | not covered |
| 315 | Alps FWB1U545 Car Remote | not covered |
| 316 | Continental KR5V2X Car Remote (-f 313.8M -s 1024k) | not covered |
| 317 | Code Alarm FRDPC2002 Car Remote | not covered |
| 327 | Honda Keyfob KR5V2X/1X (-f 433.6M -s 1024k) | not covered |
| 329 | Microchip HCS362 KeeLoq PWM | not covered |
| 330 | Microchip HCS362 KeeLoq MC | not covered |
| 333 | Opel Mokka Car Key | not covered |
| 360 | Baldr HCS528ARF Pool Thermometer sensor | not covered |
| 370 | 2GIG-KEY2E-345 encrypted keyfob | not covered (disabled by default in rtl_433) |
| 381 | Honda (TRW PPA-GF33) TPMS | not covered |

## Other (4 of 48 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 6 | ELV EM 1000 | not covered (disabled by default in rtl_433) |
| 7 | ELV WS 2000 | not covered (disabled by default in rtl_433) |
| 13 | Mebus 433 | not covered (disabled by default in rtl_433) |
| 17 | Cardin S466-TX2 | not covered |
| 21 | Calibeur RF-104 Sensor | not covered |
| 24 | Brennenstuhl RCS 2044 | not covered (disabled by default in rtl_433) |
| 25 | Globaltronics GT-WT-02 Sensor | covered: GT-WT-02 |
| 31 | TFA-Twin-Plus-30.3049, Conrad KW9010, Ea2 BL999 | not covered |
| 33 | WT450, WT260H, WT405H | not covered |
| 35 | Esperanza EWS | not covered |
| 36 | Efergy e2 classic | covered: Efergy e2 classic |
| 49 | Quhwa | not covered |
| 58 | Blyss DC5-UK-WH | not covered |
| 63 | Efergy Optical | not covered |
| 67 | Radiohead ASK | not covered |
| 72 | RF-tech | not covered (disabled by default in rtl_433) |
| 73 | LaCrosse TX141-Bv2, TX141TH-Bv2, TX141-Bv3, TX141W, TX145wsdth, (TFA, ORIA) sensor | covered: LaCrosse TX141 family |
| 77 | Vaillant calorMatic VRT340f Central Heating Control | not covered |
| 80 | IBIS beacon | not covered |
| 85 | Solight TE44/TE66, EMOS E0107T, NX-6876-917 | covered: Solight TE44 |
| 98 | GE Color Effects | not covered |
| 103 | Sensible Living Mini-Plant Moisture Sensor | not covered |
| 114 | Maverick ET73 | not covered |
| 138 | Globaltronics GT-WT-03 Sensor | not covered |
| 139 | Norgo NGE101 | not covered |
| 150 | Klimalogg | not covered (disabled by default in rtl_433) |
| 154 | Standard Consumption Message Plus (SCMplus) | not covered |
| 159 | Insteon | not covered |
| 170 | LaCrosse Technology View LTV-WR1 Multi Sensor | not covered |
| 188 | Marlec Solar iBoost+ sensors | not covered |
| 198 | EnOcean ERP1 | not covered (disabled by default in rtl_433) |
| 202 | Funkbus / Instafunk (Berker, Gira, Jung) | not covered |
| 216 | ANT and ANT+ devices | not covered (disabled by default in rtl_433) |
| 230 | Vauno EN8822C | not covered |
| 233 | CED7000 Shot Timer | not covered (disabled by default in rtl_433) |
| 234 | Watchman Sonic Advanced / Plus, Tekelek | not covered |
| 264 | Arexx Multilogger IP-HA90, IP-TH78EXT, TSN-70E | not covered |
| 265 | Rosstech Digital Control Unit DCU-706/Sundance/Jacuzzi | not covered |
| 269 | DeltaDore X3D devices | not covered |
| 270 | Quinetic | not covered (disabled by default in rtl_433) |
| 285 | bm5-v2 12V Battery Monitor | not covered |
| 296 | Apator Metra E-ITN 30 heat cost allocator | not covered |
| 323 | LaCrosse Technology TX22U-IT | not covered |
| 324 | LaCrosse Technology TX22U-IT (OOK) | not covered |
| 337 | Omni Multisensor | not covered |
| 345 | FSL Cricket Scoreboard Controller | not covered |
| 347 | TFA Dostmann 30.390X T/H sensors series | not covered |
| 364 | Elsner Solexa 230V | not covered |

## Remotes, switches, blinds (2 of 33 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 1 | Silvercrest Remote Control | not covered |
| 4 | Waveman Switch Transmitter | not covered |
| 14 | Intertechno 433 | not covered (disabled by default in rtl_433) |
| 15 | KlikAanKlikUit Wireless Switch | not covered |
| 20 | Ambient Weather F007TH, TFA 30.3208.02, SwitchDocLabs F016TH temperature sensor | covered: Ambient F007TH |
| 22 | X10 RF | not covered |
| 30 | Generic Remote SC226x EV1527 | covered: Generic remote EV1527 / PT2262 |
| 46 | HT680 Remote control | not covered |
| 48 | Akhan 100F14 remote keyless entry | not covered (disabled by default in rtl_433) |
| 51 | Proove / Nexa / KlikAanKlikUit Wireless Switch | not covered |
| 54 | Oregon Scientific SL109H Remote Thermal Hygro Sensor | not covered |
| 61 | LightwaveRF | not covered (disabled by default in rtl_433) |
| 96 | Nexa | not covered |
| 101 | Dish remote 6.3 | not covered (disabled by default in rtl_433) |
| 108 | Hyundai WS SENZOR Remote Temperature Sensor | not covered |
| 122 | FS20 / FHT | not covered (disabled by default in rtl_433) |
| 128 | DirecTV RC66RX Remote Control | not covered |
| 142 | Fine Offset Electronics/Ecowitt WH51, WN31, SwitchDoc Labs SM23 Soil Moisture Sensor | not covered |
| 153 | Cotech 36-7959, SwitchDocLabs FT020T wireless weather station with USB | not covered |
| 167 | Somfy RTS | not covered |
| 169 | Nice Flor-s remote control for gates | not covered (disabled by default in rtl_433) |
| 174 | EcoDHOME Smart Socket and MCEE Solar monitor | not covered |
| 187 | RojaFlex shutter and remote devices | not covered |
| 189 | Somfy io-homecontrol | not covered |
| 199 | Linear Megacode Garage/Gate Remotes | not covered (disabled by default in rtl_433) |
| 207 | SmartFire Proflame 2 remote control | not covered |
| 211 | Regency Ceiling Fan Remote (-f 303.75M to 303.96M) | not covered |
| 227 | SRSmith Pool Light Remote Control SRS-2C-TX (-f 915M) | not covered |
| 286 | Universal (Reverseable) 24V Fan Controller | not covered |
| 332 | TR-502MSV remote smart socket controller | not covered |
| 335 | Martec MPLCD Ceiling Fan Remote | not covered |
| 358 | Eberle Instat 868r1 floor heating thermostat remote | not covered |
| 372 | Elero bidirectional blinds/awning remote (Silent Gliss and others) | not covered |

## Security, door / window, PIR, doorbell, smoke (4 of 37 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 23 | DSC Security Contact | not covered |
| 29 | Chuango Security Technology | not covered (disabled by default in rtl_433) |
| 40 | Acurite 592TXR temp/humidity, 592TX temp, 5n1, 3n1, Atlas weather station, 515 fridge/freezer, 6045 lightning, 899 rain, 1190/1192 leak | covered: Acurite 592TXR tower and 5-in-1 (not Atlas, 515, 6045, 899, 1190) |
| 62 | Elro DB286A Doorbell | not covered (disabled by default in rtl_433) |
| 68 | Kerui PIR / Contact Sensor | covered: Kerui PIR / contact |
| 70 | Honeywell Door/Window Sensor, 2Gig DW10/DW11, RE208 repeater | covered: Honeywell door/window |
| 86 | Wireless Smoke and Heat Detector GS 558 | not covered (disabled by default in rtl_433) |
| 87 | Generic wireless motion sensor | not covered |
| 94 | Philips outdoor temperature sensor (type AJ3650) | not covered |
| 99 | X10 Security | not covered |
| 100 | Interlogix GE UTC Security Devices | not covered |
| 102 | SimpliSafe Home Security System | not covered |
| 115 | Honeywell ActivLink, Wireless Doorbell | not covered |
| 116 | Honeywell ActivLink, Wireless Doorbell (FSK) | not covered |
| 132 | TFA Dostmann 30.3196 T/H outdoor sensor | not covered |
| 135 | Philips outdoor temperature sensor (type AJ7010) | not covered |
| 148 | DSC Security Contact (WS4945) | not covered |
| 165 | TFA Dostmann 30.3221.02 T/H Outdoor Sensor (also 30.3249.02) | not covered |
| 172 | Bresser Weather Center 6-in-1, 7-in-1 indoor, soil, new 5-in-1, 3-in-1 wind gauge, Froggit WH6000, Ventus C8488A | covered: Bresser 6-in-1 |
| 179 | Cavius smoke, heat and water detector | not covered |
| 191 | Markisol, E-Motion, BOFU, Rollerhouse, BF-30x, BF-415 curtain remote | not covered (disabled by default in rtl_433) |
| 192 | Govee Water Leak Detector H5054, Door Contact Sensor B5023 | not covered |
| 209 | SimpliSafe Gen 3 Home Security System | not covered |
| 210 | Yale HSA (Home Security Alarm), YES-Alarmkit | not covered |
| 231 | Govee Water Leak Detector H5054 | not covered |
| 247 | Bresser water leakage | not covered |
| 251 | Fine Offset / Ecowitt WH55 water leak sensor | not covered |
| 258 | Chamberlain CWPIRC PIR Sensor | not covered |
| 261 | Geevon TX16-3 outdoor sensor | not covered |
| 266 | Risco 2 Way Agility protocol, Risco PIR/PET Sensor RWX95P | not covered |
| 291 | Geevon TX19-1 outdoor sensor | not covered |
| 292 | WallarGe CLTX001 Outdoor Temperature Sensor | not covered |
| 300 | Govee Water Leak Detector H5059 | not covered |
| 342 | Vivint Door/Window Sensor, V-DW21R-345/V-DW11-345 | not covered |
| 361 | Kidde RF-SM-DC wireless-interconnect smoke alarm | not covered (disabled by default in rtl_433) |
| 368 | Cavius Door/Window sensor (-f 868.7M) | not covered |
| 386 | Olympia Protect 9571 alarm system sensors | not covered |

## TPMS (9 of 39 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 59 | Steelmate TPMS | not covered |
| 60 | Schrader TPMS | covered: Schrader TPMS |
| 82 | Citroen TPMS | covered: Citroen TPMS |
| 88 | Toyota TPMS | covered: Toyota TPMS |
| 89 | Ford TPMS | covered: Ford TPMS |
| 90 | Renault TPMS | covered: Renault TPMS |
| 95 | Schrader TPMS EG53MA4, Saab, Opel, Vauxhall, Chevrolet | covered: Schrader EG53MA4 |
| 110 | PMV-107J (Toyota) TPMS | covered: PMV-107J TPMS |
| 123 | Jansite TPMS Model TY02S | not covered (disabled by default in rtl_433) |
| 140 | Elantra2012 TPMS | not covered |
| 156 | Abarth 124 Spider and Shenzhen EGQ Q85 TPMS | covered: Abarth 124 TPMS |
| 168 | Schrader TPMS SMD3MA4 (Subaru) | not covered |
| 180 | Jansite TPMS Model Solar | not covered |
| 186 | Hyundai TPMS (VDO) | covered: Hyundai TPMS (VDO) |
| 201 | Unbranded SolarTPMS for trucks | not covered |
| 203 | Porsche Boxster/Cayman TPMS | not covered |
| 208 | AVE TPMS | not covered |
| 212 | Renault 0435R TPMS | not covered |
| 225 | TyreGuard 400 TPMS | not covered |
| 226 | Kia TPMS (-s 1000k) | not covered |
| 241 | EezTire E618, Carchet TPMS, TST-507 TPMS | not covered |
| 248 | Nissan TPMS | not covered |
| 252 | BMW Gen4-Gen5 TPMS and Audi TPMS Pressure Alert, multi-brand HUF/Beru, Continental, Schrader/Sensata, Audi | not covered |
| 257 | BMW Gen2 and Gen3 TPMS | not covered |
| 275 | GM-Aftermarket TPMS | not covered |
| 295 | Airpuxem TPMS TYH11_EU6_ZQ | not covered |
| 298 | TRW TPMS OOK OEM and Clone models | not covered |
| 299 | TRW TPMS FSK OEM and Clone models | not covered |
| 321 | Schrader Motorcycle TPMS sensor | not covered |
| 322 | Gear Hive TPMS sensor | not covered |
| 328 | Schrader TPMS MRXBC5A4 (BMW) | not covered |
| 343 | SmarTire TPMS sensor, Aston Martin/Vantage DB9 protocol | not covered |
| 352 | Schrader TPMS MRXNIS315G3, 3039 (Infiniti, Nissan, Renault), aka Redi-Sensor SE10001HP/SE10001HPR | not covered |
| 354 | iMars T240 TPMS | not covered |
| 355 | Jansite TPMS TY-468-eu2 / KKMOON TPMS | not covered |
| 362 | Jansite TPMS TY588-EU2 | not covered |
| 365 | Mercedes Benz Sprinter 4500 TPMS sensor | not covered |
| 378 | Sefis M3 / Careud / Sykik SRTP300 TPMS | not covered |
| 380 | Jeep TPMS | not covered (disabled by default in rtl_433) |

## Utility and energy meters (8 of 77 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 41 | Acurite 986 Refrigerator / Freezer Thermometer | not covered |
| 44 | CurrentCost Current Sensor | covered: CurrentCost |
| 45 | emonTx OpenEnergyMonitor | not covered |
| 84 | Thermopro TP11 Thermometer | covered: ThermoPro TP11 |
| 97 | ThermoPro TP08/TP12/TP20 thermometer | covered: ThermoPro TP08 / TP12 / TP20 |
| 104 | Wireless M-Bus, Mode C&T, 100kbps (-f 868.95M -s 1200k) | covered: Wireless M-Bus C and T (header) |
| 105 | Wireless M-Bus, Mode S, 32.768kbps (-f 868.3M -s 1000k) | covered: Wireless M-Bus S (header) |
| 106 | Wireless M-Bus, Mode R, 4.8kbps (-f 868.33M) | not covered (disabled by default in rtl_433) |
| 107 | Wireless M-Bus, Mode F, 2.4kbps | not covered (disabled by default in rtl_433) |
| 109 | WT0124 Pool Thermometer | not covered |
| 113 | Ambient Weather WH31E Thermo-Hygrometer Sensor, EcoWitt WH40/WN20 rain gauge, WS68 weather station | not covered |
| 117 | ESA1000 / ESA2000 Energy Monitor, GIRA Wetterstation | not covered (disabled by default in rtl_433) |
| 125 | TS-FT002 Wireless Ultrasonic Tank Liquid Level Meter With Temperature Sensor | not covered |
| 127 | Ecowitt Wireless Outdoor Thermometer WH53/WH0280/WH0281A | not covered |
| 130 | IKEA Sparsnas Energy Meter Monitor | not covered |
| 133 | Rubicson 48659 Thermometer | not covered |
| 136 | ESIC EMT7110 power meter | not covered |
| 149 | ERT Standard Consumption Message (SCM) | covered: ERT SCM |
| 151 | Visonic powercode | not covered |
| 160 | ERT Interval Data Message (IDM) | covered: ERT IDM |
| 161 | ERT Interval Data Message (IDM) for Net Meters | not covered |
| 176 | BlueLine Innovations Power Cost Monitor, Black & Decker EM100B | not covered |
| 177 | Burnhard BBQ thermometer | not covered |
| 181 | Amazon Basics Meat Thermometer | not covered |
| 182 | TFA Marbella Pool Thermometer | not covered |
| 193 | Clipsal CMR113 Cent-a-meter power meter | not covered |
| 197 | Acurite Grill/Meat Thermometer 01185M | not covered |
| 204 | Jasco/GE Choice Alert Security Devices | not covered |
| 222 | Rubicson Pool Thermometer 48942 | not covered |
| 223 | Badger ORION water meter, 100kbps (-f 916.45M -s 1200k) | not covered |
| 224 | GEO minim+ energy monitor | not covered |
| 228 | Neptune R900 flow meters | not covered |
| 232 | TFA Dostmann 14.1504.V2 Radio-controlled grill and meat thermometer | not covered |
| 236 | Gasmate BA1008 meat thermometer | not covered |
| 237 | Flowis flow meters | not covered |
| 238 | Wireless M-Bus, Mode T, 32.768kbps (-f 868.3M -s 1000k) | covered: Wireless M-Bus T (header) |
| 239 | Revolt NC-5642 Energy Meter | not covered |
| 245 | ThermoPro TX-2C, TX-2B Thermometer and Humidity sensor | not covered (disabled by default in rtl_433) |
| 250 | Schou 72543 Day Rain Gauge, Motonet MTX Rain, MarQuant Rain Gauge, TFA Dostmann 30.3252.01/47.3006.01 Rain Gauge and Thermometer, ADE WS1907 | not covered |
| 255 | Mueller Hot Rod water meter | not covered |
| 256 | ThermoPro TP28b Super Long Range Wireless Meat Thermometer for Smoker BBQ Grill | not covered |
| 259 | ThermoPro Meat Thermometers, TP829B 4 probes with temp only | not covered |
| 260 | Arad/Master Meter Dialog3G water utility meter | not covered |
| 267 | ThermoPro Meat Thermometers, TP828B 2 probes with Temp, BBQ Target LO and HI | not covered |
| 271 | Landis & Gyr Gridstream Power Meters 9.6k | not covered |
| 272 | Landis & Gyr Gridstream Power Meters 19.2k | not covered |
| 273 | Landis & Gyr Gridstream Power Meters 38.4k | not covered |
| 274 | Revolt ZX-7717 power meter | not covered |
| 277 | Apator Metra E-RM 30 water meter | not covered |
| 278 | ThermoPro TX-7B Outdoor Thermometer Hygrometer | not covered |
| 280 | Homelead HG9901 (Geevon, Dr.Meter, Royal Gardineer) soil moisture/temp/light level sensor | not covered |
| 282 | Orion Endpoint from Badger Meter, GIF2014W-OSE, water meter, hopping | not covered |
| 284 | Baldr E0666TH Thermo-Hygrometer | not covered |
| 288 | Oria WA150KM freezer and fridge thermometer | not covered |
| 289 | Voltcraft EnergyCount 3000 (ec3k) | not covered |
| 290 | Orion Endpoint from Badger Meter, GIF2020OCECNA, water meter, hopping | not covered |
| 294 | ThermoPro TempSpike XR TP862b / TP863b Wireless Dual-Probe Meat Thermometer | not covered |
| 297 | ThermoPro TP211B Thermometer | not covered |
| 318 | RFM69 LowPowerLab Moteino board (-s 1000k) | not covered |
| 320 | Cellular Tracking Technologies LifeTag/PowerTag/HybridTag | not covered |
| 326 | Acurite/Chaney 985 Refrigerator / Freezer Thermometer | not covered |
| 331 | RF-T0912 Grill Thermometer | not covered |
| 334 | RfxMeter, RFXPwr | not covered |
| 338 | Elster/Honeywell R2S/REXU power meter | not covered |
| 339 | Florabest FB-TH-1 BBQ Thermometer | not covered |
| 341 | Esun EN2053 two-channel BBQ thermometer | not covered |
| 344 | Dickert MAHS433-01 garage door remote control | not covered (disabled by default in rtl_433) |
| 349 | Govee Pool/Spa Thermometer H5310 | not covered |
| 350 | Govee H5112 Dual-Probe Thermometer | not covered |
| 351 | Eco-Eye solar PV/grid current monitor | not covered |
| 369 | Cotech FT0203/18-3676 anemometer | not covered (disabled by default in rtl_433) |
| 373 | Elster/Honeywell R2S/REXU power meter, type-2 frames | not covered |
| 376 | McPower Kinetic battery-less wall switch | not covered |
| 377 | Typhur Sync Gold meat thermometer probe | not covered |
| 379 | RADIAN/RADIAN0 meter | not covered |
| 383 | Silver Spring Networks mesh endpoint (-s 1600k) | not covered |
| 385 | Neptune R900 BCD flow meters | not covered (disabled by default in rtl_433) |

## Weather, thermometers, probes, sensors (18 of 115 covered)

| rtl_433 # | Device | Status |
|---|---|---|
| 2 | Rubicson, TFA 30.3197 or InFactory PT-310 Temperature Sensor | covered: Rubicson |
| 3 | Prologue, FreeTec NC-7104, NC-7159-675 temperature sensor | covered: Prologue |
| 8 | LaCrosse TX Temperature / Humidity Sensor, TX5U rain gauge | not covered |
| 10 | Acurite 896 Rain Gauge | not covered |
| 11 | Acurite 609TXC Temperature and Humidity Sensor | covered: Acurite 609TXC |
| 12 | Oregon Scientific Weather Sensor | covered: Oregon Scientific v2.1 / v3 (not v1, not WMR500) |
| 16 | AlectoV1 Weather Sensor (Alecto WS3500 WS4500 Ventus W155/W044 Oregon) | not covered |
| 18 | Fine Offset Electronics, WH2, WH5, Telldus Temperature/Humidity/Rain Sensor | covered: Fine Offset WH2 / WH2A / WH5 |
| 19 | Nexus, FreeTec NC-7345, NX-3980, Solight TE82S, TFA 30.3209 temperature/humidity sensor | covered: Nexus |
| 26 | Danfoss CFR Thermostat | not covered |
| 32 | Fine Offset Electronics WH1080/WH3080 Weather Station | covered: Fine Offset WH1080 / WH3080 |
| 34 | LaCrosse WS-2310 / WS-3600 Weather Station | not covered |
| 37 | Inovalley kw9015b, TFA Dostmann 30.3161 (Rain and temperature sensor) | not covered (disabled by default in rtl_433) |
| 38 | Generic temperature sensor 1 | covered: Generic temperature sensor 1 |
| 39 | WG-PB12V1 Temperature Sensor | not covered |
| 42 | HIDEKI TS04 Temperature, Humidity, Wind and Rain Sensor | not covered |
| 43 | Watchman Sonic / Apollo Ultrasonic / Beckett Rocket oil tank monitor | not covered |
| 47 | Conrad S3318P, FreeTec NC-5849-913 temperature humidity sensor, ORIA WA50 ST389 temperature sensor | not covered |
| 50 | OSv1 Temperature Sensor | not covered |
| 52 | Bresser Thermo-/Hygro-Sensor 3CH | covered: Bresser 3CH |
| 53 | Springfield Temperature and Soil Moisture | covered: Springfield soil moisture |
| 55 | Acurite 606TX / Technoline TX960 Temperature Sensor | covered: Acurite 606TX |
| 56 | TFA pool temperature sensor | not covered |
| 57 | Kedsum Temperature & Humidity Sensor, Pearl NC-7415 | covered: Kedsum |
| 69 | Fine Offset WH1050 Weather Station | not covered |
| 71 | Maverick ET-732/733 BBQ Sensor | not covered |
| 74 | Acurite 00275rm,00276rm Temp/Humidity with optional probe | not covered |
| 75 | LaCrosse TX35DTH-IT, TFA Dostmann 30.3155 Temperature/Humidity sensor | covered: LaCrosse TX35DTH-IT |
| 76 | LaCrosse TX29IT, TFA Dostmann 30.3159.IT Temperature sensor | covered: LaCrosse TX29IT |
| 78 | Fine Offset Electronics, WH25, WH32, WH32B, WN32B, WH24, WH65, WS69, HP1000, Misol WS2320 Temperature/Humidity/Pressure Sensor | covered: Fine Offset WH24 / WH65 |
| 79 | Fine Offset Electronics, WH0530 Temperature/Rain Sensor | not covered |
| 81 | Oil Ultrasonic STANDARD FSK | not covered |
| 83 | Oil Ultrasonic STANDARD ASK | not covered |
| 91 | inFactory, nor-tec, FreeTec NC-3982-913 temperature humidity sensor | not covered |
| 92 | FT-004-B Temperature Sensor | covered: FT-004-B |
| 111 | Emos TTX201 Temperature Sensor | not covered |
| 112 | Ambient Weather TX-8300 Temperature/Humidity Sensor | not covered |
| 118 | Biltema rain gauge | not covered (disabled by default in rtl_433) |
| 119 | Bresser Weather Center 5-in-1 | covered: Bresser 5-in-1 |
| 120 | Digitech XC-0324 / AmbientWeather FT005TH temp/hum sensor | not covered |
| 121 | Opus/Imagintronix XT300 Soil Moisture | not covered |
| 124 | LaCrosse/ELV/Conrad WS7000/WS2500 weather sensors | not covered |
| 126 | Companion WTR001 Temperature Sensor | not covered |
| 129 | Eurochron temperature and humidity sensor | not covered (disabled by default in rtl_433) |
| 134 | AOK Weather Station rebrand Holman Industries iWeather WS5029, Conrad AOK-5056, Optex 990018 | not covered |
| 137 | Globaltronics QUIGG GT-TMBBQ-05 | not covered |
| 141 | Auriol HG02832, HG05124A-DCF, Rubicson 48957 temperature/humidity sensor | not covered |
| 143 | Holman Industries iWeather WS5029 weather station (older PWM) | not covered |
| 144 | TBH weather sensor | not covered |
| 145 | WS2032 weather station | not covered |
| 146 | Auriol AFW2A1 temperature/humidity sensor | not covered |
| 147 | TFA Drop Rain Gauge 30.3233.01 | not covered |
| 152 | Eurochron EFTH-800 temperature and humidity sensor | not covered |
| 155 | Fine Offset Electronics WH1080/WH3080 Weather Station (FSK) | not covered |
| 157 | Missil ML0757 weather station | not covered |
| 158 | Sharp SPC775 weather station | not covered |
| 162 | ThermoPro-TX2 temperature sensor | not covered (disabled by default in rtl_433) |
| 163 | Acurite 590TX Temperature with optional Humidity | not covered |
| 166 | LaCrosse Technology View LTV-WSDTH01 Breeze Pro Wind Sensor | not covered |
| 171 | LaCrosse Technology View LTV-TH Thermo/Hygro Sensor | not covered |
| 173 | Bresser Weather Center 7-in-1, Air Quality PM2.5/PM10 7009970, CO2 7009977, HCHO/VOC 7009978 sensors | covered: Bresser 7-in-1 |
| 175 | LaCrosse Technology View LTV-R1, LTV-R3 Rainfall Gauge, LTV-W1/W2 Wind Sensor | not covered |
| 183 | Auriol AHFL temperature/humidity sensor | not covered |
| 184 | Auriol AFT 77 B2 temperature sensor | not covered |
| 185 | Honeywell CM921 Wireless Programmable Room Thermostat | not covered |
| 190 | Ambient Weather WH31L (FineOffset WH57) Lightning-Strike sensor | not covered |
| 194 | Inkbird ITH-20R temperature humidity sensor | not covered |
| 195 | RainPoint soil temperature and moisture sensor | not covered |
| 196 | Atech-WS308 temperature sensor | not covered |
| 200 | Auriol 4-LD5661/4-LD5972/4-LD6313, Sempre 4-AH0423-4 temperature/rain sensors | not covered (disabled by default in rtl_433) |
| 205 | Telldus weather station FT0385R sensors | not covered |
| 206 | LaCrosse TX34-IT rain gauge | not covered |
| 213 | Fine Offset Electronics WS80 weather station | not covered |
| 214 | EMOS E6016 weatherstation with DCF77, EMOS E6018 temperature/humidity sensor | not covered |
| 215 | Emax W6 and rebrands, Weather Station or temperature/humidity sensor | not covered |
| 217 | EMOS E6016 rain gauge | not covered |
| 219 | Fine Offset Electronics WH45 air quality sensor | not covered |
| 220 | Maverick XR-30 BBQ Sensor | not covered |
| 221 | Fine Offset Electronics WN34S/L/D, WN38 and Froggit DP150/D35 temperature sensor | not covered |
| 229 | WEC-2103 temperature/humidity sensor | not covered |
| 235 | Oil Ultrasonic SMART FSK | not covered |
| 240 | LaCrosse TX31U-IT, The Weather Channel WS-1910TWC-IT | not covered |
| 242 | Baldr / RainPoint rain gauge. | not covered (disabled by default in rtl_433) |
| 243 | Celsia CZC1 Thermostat | not covered |
| 244 | Fine Offset Electronics WS90 weather station | not covered |
| 246 | TFA 30.3151 Weather Station | not covered |
| 249 | Bresser lightning | not covered |
| 253 | Watts WFHT-RF Thermostat | not covered |
| 254 | Thermor DG950 weather station | not covered |
| 262 | Fine Offset Electronics WH46 air quality sensor | not covered |
| 263 | Vevor Wireless Weather Station 7-in-1 | not covered |
| 268 | Bresser Thermo-/Hygro-Sensor Explore Scientific ST1005H | not covered |
| 279 | Nexus, CRX, Prego sauna temperature sensor | not covered |
| 281 | Maverick XR-50 BBQ Sensor | not covered |
| 283 | Fine Offset Electronics WH43 air quality sensor | not covered |
| 287 | Fine Offset Electronics WS85 weather station | not covered |
| 293 | Sainlogic SA8, Gevanti SA8 Weather Station | not covered |
| 319 | Shenzhen Wale WL-TH6R Temperature & Humidity Sensor | not covered |
| 325 | Thermor A6N 132TX temperature sensor | not covered |
| 336 | NetAtmo temp/hum and wind sensors | not covered |
| 340 | Holman Industries iWeather WS5029 weather station (older PWM, OOK), BIOWIN 270208 | not covered |
| 346 | EN2058 four probe temperature sensor | not covered |
| 348 | Watts WFHT-RF / WFHC-MASTERH&C-RF underfloor heating thermostat | not covered |
| 353 | Fine Offset Electronics / Ecowitt WH52 Soil Moisture/Temperature/EC Sensor | not covered |
| 356 | Agimex Rosenborg 66796 (collides with Fine Offset Electronics WH5) Temperature/Humidity Sensor | not covered (disabled by default in rtl_433) |
| 357 | Auriol HG04641A temperature station | not covered |
| 359 | Kingspan/Watchman Plus (Niveau) oil tank monitor | not covered |
| 363 | Oregon Scientific WMR500 weather station | not covered |
| 366 | Watts Vision thermostat (-f 868.3M) | not covered |
| 367 | TFA 30.3307.02 Wind sensor | not covered |
| 371 | Hanwell ML/RL4000-series Radiologger temperature/humidity sensor | not covered (disabled by default in rtl_433) |
| 374 | LaCrosse WS6868 TX232TH-LCD temperature/humidity sensor | not covered |
| 375 | LaCrosse WS6868 TX231RW wind/rain sensor | not covered |
| 382 | Cotech 36-7900 rain gauge | not covered (disabled by default in rtl_433) |
| 384 | Bresser SmartHome Garden soil moisture and water timer valve (Baldr Homgar, RainPoint) | not covered |
