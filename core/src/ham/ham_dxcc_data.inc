// The built-in country table (included by ham_dxcc.cpp), written for OnAir from the public ITU prefix allocations, in the cty.dat layout
// so that one reader serves it and a user's own full file. Fields: name, CQ zone, ITU zone, continent, latitude, longitude (west positive,
// as cty.dat), UTC offset (not used), primary prefix; then the prefixes. "#n" is the ADIF entity number (an OnAir extension), "?" is any
// letter. Positions are rough country centres; zones are the main zone of the entity (large countries span several: see the overrides).
R"CTY(
Canada: 05: 09: NA: 45.0: 75.0: 0: VE:
    #1,VA,VB,VC,VD,VE,VF,VG,VO,VX,VY,CF,CG,CH,CI,CJ,CK,CY,CZ,XJ,XK,XL,XM,XN,XO,VE7(3)[2],VA7(3)[2],VE8(1)[3],VY1(1)[2],VE6(4)[2],VA6(4)[2],VE5(4)[3],VA5(4)[3],VE4(4)[3],VA4(4)[3],VE3(4)[4],VA3(4)[4],VO2(2)[9],VY0(2)[4];
United States: 05: 08: NA: 38.0: 97.0: 0: K:
    #291,K,W,N,AA,AB,AC,AD,AE,AF,AG,AI,AJ,AK,K6(3)[6],W6(3)[6],N6(3)[6],K7(3)[6],W7(3)[6],N7(3)[6],K0(4)[7],W0(4)[7],N0(4)[7],K5(4)[7],W5(4)[7],N5(4)[7],K9(4)[8],W9(4)[8],N9(4)[8],K8(5)[8],W8(5)[8],N8(5)[8];
Alaska: 01: 01: NA: 61.4: 149.9: 0: KL:
    #6,AL,KL,NL,WL;
Hawaii: 31: 61: OC: 21.3: 157.9: 0: KH6:
    #110,AH6,AH7,KH6,KH7,NH6,NH7,WH6,WH7;
Puerto Rico: 08: 11: NA: 18.2: 66.5: 0: KP4:
    #202,KP3,KP4,NP3,NP4,WP3,WP4;
US Virgin Islands: 08: 11: NA: 18.3: 64.9: 0: KP2:
    #285,KP2,NP2,WP2;
Guam: 27: 64: OC: 13.5: -144.8: 0: KH2:
    #103,AH2,KH2,NH2,WH2;
American Samoa: 32: 62: OC: -14.3: 170.7: 0: KH8:
    #9,AH8,KH8,NH8,WH8;
Mexico: 06: 10: NA: 21.3: 101.4: 0: XE:
    #50,XA,XB,XC,XD,XE,XF,XG,XH,XI,4A,4B,4C,6D,6E,6F,6G,6H,6I,6J;
Cuba: 08: 11: NA: 22.0: 79.5: 0: CO:
    #70,CL,CM,CO,T4;
Dominican Republic: 08: 11: NA: 18.8: 70.2: 0: HI:
    #72,HI;
Haiti: 08: 11: NA: 19.0: 72.4: 0: HH:
    #78,HH,4V;
Jamaica: 08: 11: NA: 18.2: 77.4: 0: 6Y:
    #82,6Y;
Bahamas: 08: 11: NA: 24.3: 76.0: 0: C6:
    #60,C6;
Bermuda: 05: 11: NA: 32.3: 64.8: 0: VP9:
    #64,VP9;
Cayman Islands: 08: 11: NA: 19.3: 81.3: 0: ZF:
    #69,ZF;
Costa Rica: 07: 11: NA: 10.0: 84.0: 0: TI:
    #308,TI,TE;
Panama: 07: 11: NA: 8.8: 80.0: 0: HP:
    #88,HO,HP,H3,H8,H9,3E,3F;
Guatemala: 07: 11: NA: 15.5: 90.3: 0: TG:
    #76,TD,TG;
Honduras: 07: 11: NA: 14.8: 86.5: 0: HR:
    #80,HQ,HR;
El Salvador: 07: 11: NA: 13.7: 88.9: 0: YS:
    #74,HU,YS;
Nicaragua: 07: 11: NA: 12.9: 85.2: 0: YN:
    #86,HT,H6,H7,YN;
Belize: 07: 11: NA: 17.2: 88.7: 0: V3:
    #66,V3;
Trinidad and Tobago: 09: 11: SA: 10.4: 61.3: 0: 9Y:
    #90,9Y,9Z;
Barbados: 08: 11: NA: 13.2: 59.5: 0: 8P:
    #62,8P;
Aruba: 09: 11: SA: 12.5: 70.0: 0: P4:
    #91,P4;
Curacao: 09: 11: SA: 12.2: 69.0: 0: PJ2:
    #517,PJ2;
Bonaire: 09: 11: SA: 12.2: 68.3: 0: PJ4:
    #520,PJ4;
Guadeloupe: 08: 11: NA: 16.2: 61.5: 0: FG:
    #79,FG;
Martinique: 08: 11: NA: 14.6: 61.0: 0: FM:
    #84,FM;
St. Lucia: 08: 11: NA: 13.9: 61.0: 0: J6:
    #97,J6;
Grenada: 08: 11: NA: 12.1: 61.7: 0: J3:
    #77,J3;
St. Vincent: 08: 11: NA: 13.2: 61.2: 0: J8:
    #98,J8;
Antigua and Barbuda: 08: 11: NA: 17.1: 61.8: 0: V2:
    #94,V2;
St. Kitts and Nevis: 08: 11: NA: 17.3: 62.7: 0: V4:
    #249,V4;
British Virgin Islands: 08: 11: NA: 18.4: 64.6: 0: VP2V:
    #65,VP2V;
Turks and Caicos: 08: 11: NA: 21.8: 71.8: 0: VP5:
    #89,VP5;
Greenland: 40: 05: NA: 72.0: 40.0: 0: OX:
    #237,OX,XP;
St. Pierre and Miquelon: 05: 09: NA: 46.8: 56.2: 0: FP:
    #277,FP;
Argentina: 13: 14: SA: -34.6: 58.4: 0: LU:
    #100,AY,AZ,LO,LP,LQ,LR,LS,LT,LU,LV,LW,L2,L3,L4,L5,L6,L7,L8,L9;
Brazil: 11: 15: SA: -15.8: 47.9: 0: PY:
    #108,PP,PQ,PR,PS,PT,PU,PV,PW,PX,PY,ZV,ZW,ZX,ZY,ZZ;
Chile: 12: 14: SA: -33.5: 70.7: 0: CE:
    #112,CA,CB,CC,CD,CE,XQ,XR,3G;
Easter Island: 12: 63: SA: -27.1: 109.4: 0: CE0Y:
    #47,CE0Y,XQ0Y,XR0Y,3G0Y;
Colombia: 09: 12: SA: 4.6: 74.1: 0: HK:
    #116,HJ,HK,5J,5K;
Ecuador: 10: 12: SA: -0.2: 78.5: 0: HC:
    #120,HC,HD;
Galapagos Islands: 10: 12: SA: -0.9: 90.9: 0: HC8:
    #71,HC8,HD8;
Peru: 10: 12: SA: -12.0: 77.0: 0: OA:
    #136,OA,OB,OC,4T;
Venezuela: 09: 12: SA: 10.5: 66.9: 0: YV:
    #148,YV,YW,YX,YY,4M;
Uruguay: 13: 14: SA: -34.9: 56.2: 0: CX:
    #144,CV,CW,CX;
Paraguay: 11: 14: SA: -25.3: 57.6: 0: ZP:
    #132,ZP;
Bolivia: 10: 12: SA: -16.5: 68.1: 0: CP:
    #104,CP;
Guyana: 09: 12: SA: 6.8: 58.2: 0: 8R:
    #129,8R;
Suriname: 09: 12: SA: 5.8: 55.2: 0: PZ:
    #140,PZ;
French Guiana: 09: 12: SA: 4.9: 52.3: 0: FY:
    #63,FY;
Falkland Islands: 13: 16: SA: -51.7: 57.9: 0: VP8:
    #141,VP8;
England: 14: 27: EU: 52.0: 1.5: 0: G:
    #223,G,M,2E,2D0;
Scotland: 14: 27: EU: 56.8: 4.2: 0: GM:
    #279,GM,MM,2M,GS,MS;
Wales: 14: 27: EU: 52.3: 3.7: 0: GW:
    #294,GW,MW,2W,GC,MC;
Northern Ireland: 14: 27: EU: 54.6: 6.0: 0: GI:
    #265,GI,MI,2I,GN,MN;
Isle of Man: 14: 27: EU: 54.2: 4.5: 0: GD:
    #114,GD,MD,2D,GT,MT;
Guernsey: 14: 27: EU: 49.5: 2.6: 0: GU:
    #106,GU,MU,2U,GP,MP;
Jersey: 14: 27: EU: 49.2: 2.1: 0: GJ:
    #122,GJ,MJ,2J,GH,MH;
Ireland: 14: 27: EU: 53.3: 8.0: 0: EI:
    #245,EI,EJ;
France: 14: 27: EU: 46.0: -2.0: 0: F:
    #227,F,HW,HX,HY,TH,TM,TO,TP,TQ,TV;
Corsica: 15: 28: EU: 42.0: -9.0: 0: TK:
    #214,TK;
Germany: 14: 28: EU: 51.0: -10.0: 0: DL:
    #230,DA,DB,DC,DD,DE,DF,DG,DH,DI,DJ,DK,DL,DM,DN,DO,DP,DQ,DR,Y2,Y3,Y4,Y5,Y6,Y7,Y8,Y9;
Belgium: 14: 27: EU: 50.7: -4.5: 0: ON:
    #209,ON,OO,OP,OQ,OR,OS,OT;
Netherlands: 14: 27: EU: 52.4: -5.3: 0: PA:
    #263,PA,PB,PC,PD,PE,PF,PG,PH,PI;
Luxembourg: 14: 27: EU: 49.6: -6.1: 0: LX:
    #254,LX;
Switzerland: 14: 28: EU: 46.9: -7.4: 0: HB:
    #287,HB,HE;
Liechtenstein: 14: 28: EU: 47.1: -9.5: 0: HB0:
    #251,HB0,HE0;
Austria: 15: 28: EU: 47.3: -13.3: 0: OE:
    #206,OE;
Italy: 15: 28: EU: 42.0: -12.5: 0: I:
    #248,I,IA,IB,IC,ID,IE,IF,IG,IH,II,IJ,IK,IL,IM,IN,IO,IP,IQ,IR,IT,IU,IV,IW,IX,IY,IZ;
Sardinia: 15: 28: EU: 40.0: -9.0: 0: IS0:
    #225,IS0,IM0,IW0U,IW0V,IW0W,IW0X,IW0Y,IW0Z;
Malta: 15: 28: EU: 35.9: -14.4: 0: 9H:
    #257,9H;
San Marino: 15: 28: EU: 43.9: -12.4: 0: T7:
    #278,T7;
Vatican City: 15: 28: EU: 41.9: -12.5: 0: HV:
    #295,HV;
Monaco: 14: 27: EU: 43.7: -7.4: 0: 3A:
    #260,3A;
Andorra: 14: 27: EU: 42.5: -1.5: 0: C3:
    #203,C3;
Spain: 14: 37: EU: 40.4: 3.7: 0: EA:
    #281,AM,AN,AO,EA,EB,EC,ED,EE,EF,EG,EH;
Balearic Islands: 14: 37: EU: 39.6: -2.9: 0: EA6:
    #21,AM6,AN6,AO6,EA6,EB6,EC6,ED6,EE6,EF6,EG6,EH6;
Canary Islands: 33: 36: AF: 28.3: 15.8: 0: EA8:
    #29,AM8,AN8,AO8,EA8,EB8,EC8,ED8,EE8,EF8,EG8,EH8;
Portugal: 14: 37: EU: 39.5: 8.0: 0: CT:
    #272,CQ,CR,CS,CT;
Azores: 14: 36: EU: 38.7: 27.2: 0: CU:
    #149,CQ8,CR8,CS8,CT8,CU;
Madeira Islands: 33: 36: AF: 32.7: 16.9: 0: CT3:
    #256,CQ3,CQ9,CR3,CR9,CS3,CS9,CT3,CT9;
Gibraltar: 14: 37: EU: 36.1: 5.4: 0: ZB:
    #233,ZB,ZG;
Czech Republic: 15: 28: EU: 50.1: -14.4: 0: OK:
    #503,OK,OL;
Slovak Republic: 15: 28: EU: 48.1: -17.1: 0: OM:
    #504,OM;
Hungary: 15: 28: EU: 47.5: -19.0: 0: HA:
    #239,HA,HG;
Poland: 15: 28: EU: 52.3: -21.0: 0: SP:
    #269,HF,SN,SO,SP,SQ,SR,3Z;
Romania: 20: 28: EU: 44.4: -26.1: 0: YO:
    #275,YO,YP,YQ,YR;
Bulgaria: 20: 28: EU: 42.7: -23.3: 0: LZ:
    #212,LZ;
Greece: 20: 28: EU: 38.0: -23.7: 0: SV:
    #236,SV,SW,SX,SY,SZ,J4;
Crete: 20: 28: EU: 35.3: -25.1: 0: SV9:
    #40,SV9,SW9,SX9,SY9,SZ9,J49;
Turkey: 20: 39: AS: 39.9: -32.9: 0: TA:
    #390,TA,TB,TC,YM;
Cyprus: 20: 39: AS: 35.2: -33.4: 0: 5B:
    #215,5B,C4,H2,P3;
Serbia: 15: 28: EU: 44.8: -20.5: 0: YU:
    #296,YT,YU;
Croatia: 15: 28: EU: 45.8: -16.0: 0: 9A:
    #497,9A;
Slovenia: 15: 28: EU: 46.1: -14.5: 0: S5:
    #499,S5;
Bosnia-Herzegovina: 15: 28: EU: 43.9: -18.4: 0: E7:
    #501,E7;
North Macedonia: 15: 28: EU: 42.0: -21.4: 0: Z3:
    #502,Z3;
Montenegro: 15: 28: EU: 42.4: -19.3: 0: 4O:
    #514,4O;
Kosovo: 15: 28: EU: 42.7: -21.2: 0: Z6:
    #522,Z6;
Albania: 15: 28: EU: 41.3: -19.8: 0: ZA:
    #7,ZA;
Moldova: 16: 29: EU: 47.0: -28.8: 0: ER:
    #179,ER;
Ukraine: 16: 29: EU: 50.4: -30.5: 0: UR:
    #288,EM,EN,EO,UR,US,UT,UU,UV,UW,UX,UY,UZ;
Belarus: 16: 29: EU: 53.9: -27.6: 0: EU:
    #27,EU,EV,EW;
Lithuania: 15: 29: EU: 55.0: -24.0: 0: LY:
    #146,LY;
Latvia: 15: 29: EU: 57.0: -24.1: 0: YL:
    #145,YL;
Estonia: 15: 29: EU: 59.0: -25.0: 0: ES:
    #52,ES;
European Russia: 16: 29: EU: 55.8: -37.6: 0: UA:
    #54,R,UA,UB,UC,UD,UE,UF,UG,UH,UI;
Kaliningrad: 15: 29: EU: 54.7: -20.5: 0: UA2:
    #126,R?2,UA2,UB2,UC2,UD2,UE2,UF2,UG2,UH2,UI2;
Asiatic Russia: 17: 30: AS: 55.0: -83.0: 0: UA9:
    #15,R?8,R?9,R?0,UA8,UA9,UA0,UB8,UB9,UB0,UC8,UC9,UC0,UD8,UD9,UD0,UE8,UE9,UE0,UF8,UF9,UF0,UG8,UG9,UG0,UH8,UH9,UH0,UI8,UI9,UI0,R?0(19)[33],UA0(19)[33];
Finland: 15: 18: EU: 61.0: -25.0: 0: OH:
    #224,OF,OG,OH,OI,OJ;
Aland Islands: 15: 18: EU: 60.1: -19.9: 0: OH0:
    #5,OF0,OG0,OH0,OI0;
Sweden: 14: 18: EU: 59.3: -18.1: 0: SM:
    #284,SA,SB,SC,SD,SE,SF,SG,SH,SI,SJ,SK,SL,SM,7S,8S;
Norway: 14: 18: EU: 60.0: -10.7: 0: LA:
    #266,LA,LB,LC,LD,LE,LF,LG,LH,LI,LJ,LK,LL,LM,LN;
Svalbard: 40: 18: EU: 78.2: -15.6: 0: JW:
    #259,JW;
Denmark: 14: 18: EU: 55.7: -12.6: 0: OZ:
    #221,OU,OV,OZ,5P,5Q;
Faroe Islands: 14: 18: EU: 62.0: 6.8: 0: OY:
    #222,OW,OY;
Iceland: 40: 17: EU: 64.1: 21.9: 0: TF:
    #242,TF;
Armenia: 21: 29: AS: 40.2: -44.5: 0: EK:
    #14,EK;
Azerbaijan: 21: 29: AS: 40.4: -49.9: 0: 4J:
    #18,4J,4K;
Georgia: 21: 29: AS: 41.7: -44.8: 0: 4L:
    #75,4L;
Kazakhstan: 17: 30: AS: 48.0: -68.0: 0: UN:
    #130,UN,UO,UP,UQ;
Uzbekistan: 17: 30: AS: 41.3: -69.2: 0: UK:
    #292,UJ,UK,UL,UM;
Tajikistan: 17: 30: AS: 38.6: -68.8: 0: EY:
    #262,EY;
Turkmenistan: 17: 30: AS: 37.9: -58.4: 0: EZ:
    #280,EZ;
Kyrgyzstan: 17: 30: AS: 42.9: -74.6: 0: EX:
    #135,EX;
Japan: 25: 45: AS: 35.7: -139.7: 0: JA:
    #339,JA,JB,JC,JD,JE,JF,JG,JH,JI,JJ,JK,JL,JM,JN,JO,JP,JQ,JR,JS,7J,7K,7L,7M,7N,8J,8K,8L,8M,8N;
China: 24: 44: AS: 39.9: -116.4: 0: BY:
    #318,B,BA,BD,BG,BH,BI,BJ,BL,BT,BY,BZ,3H,3I,3J,3K,3L,3M,3N,3O,3P,3Q,3R,3S,3T,3U,XS;
Taiwan: 24: 44: AS: 25.0: -121.5: 0: BV:
    #386,BM,BN,BO,BP,BQ,BU,BV,BW,BX;
Hong Kong: 24: 44: AS: 22.3: -114.2: 0: VR:
    #321,VR;
Macao: 24: 44: AS: 22.2: -113.5: 0: XX9:
    #152,XX9;
Republic of Korea: 25: 44: AS: 37.6: -127.0: 0: HL:
    #137,DS,DT,D7,D8,D9,HL,6K,6L,6M,6N;
DPR of Korea: 25: 44: AS: 39.0: -125.8: 0: P5:
    #344,HM,P5,P6,P7,P8,P9;
Mongolia: 23: 32: AS: 47.9: -106.9: 0: JT:
    #363,JT,JU,JV;
India: 22: 41: AS: 22.0: -78.0: 0: VU:
    #324,AT,AU,AV,AW,VT,VU,VV,VW,8T,8U,8V,8W,8X,8Y;
Pakistan: 21: 41: AS: 30.0: -70.0: 0: AP:
    #372,AP,AQ,AR,AS,6P,6Q,6R,6S;
Bangladesh: 22: 41: AS: 23.7: -90.4: 0: S2:
    #305,S2,S3;
Sri Lanka: 22: 41: AS: 6.9: -79.9: 0: 4S:
    #315,4P,4Q,4R,4S;
Nepal: 22: 42: AS: 27.7: -85.3: 0: 9N:
    #369,9N;
Afghanistan: 21: 40: AS: 34.5: -69.2: 0: YA:
    #3,T6,YA;
Maldives: 22: 41: AS: 4.2: -73.5: 0: 8Q:
    #159,8Q;
Iran: 21: 40: AS: 35.7: -51.4: 0: EP:
    #330,EP,EQ,9B,9C,9D;
Iraq: 21: 39: AS: 33.3: -44.4: 0: YI:
    #333,HN,YI;
Israel: 20: 39: AS: 31.8: -35.2: 0: 4X:
    #336,4X,4Z;
Jordan: 20: 39: AS: 32.0: -35.9: 0: JY:
    #342,JY;
Lebanon: 20: 39: AS: 33.9: -35.5: 0: OD:
    #354,OD;
Syria: 20: 39: AS: 33.5: -36.3: 0: YK:
    #384,YK,6C;
Saudi Arabia: 21: 39: AS: 24.7: -46.7: 0: HZ:
    #378,HZ,7Z,8Z;
United Arab Emirates: 21: 39: AS: 24.5: -54.4: 0: A6:
    #391,A6;
Qatar: 21: 39: AS: 25.3: -51.5: 0: A7:
    #376,A7;
Bahrain: 21: 39: AS: 26.2: -50.6: 0: A9:
    #304,A9;
Kuwait: 21: 39: AS: 29.4: -48.0: 0: 9K:
    #348,9K;
Oman: 21: 39: AS: 23.6: -58.5: 0: A4:
    #370,A4;
Yemen: 21: 39: AS: 15.4: -44.2: 0: 7O:
    #492,7O;
Thailand: 26: 49: AS: 13.8: -100.5: 0: HS:
    #387,E2,HS;
Vietnam: 26: 49: AS: 21.0: -105.8: 0: XV:
    #293,XV,3W;
Cambodia: 26: 49: AS: 11.6: -104.9: 0: XU:
    #312,XU;
Laos: 26: 49: AS: 18.0: -102.6: 0: XW:
    #143,XW;
Myanmar: 26: 49: AS: 16.8: -96.2: 0: XZ:
    #309,XY,XZ;
Bhutan: 22: 41: AS: 27.5: -89.6: 0: A5:
    #306,A5;
West Malaysia: 28: 54: AS: 3.1: -101.7: 0: 9M2:
    #299,9M2,9M4,9W2,9W4;
East Malaysia: 28: 54: OC: 3.0: -113.0: 0: 9M6:
    #46,9M6,9M8,9W6,9W8;
Singapore: 28: 54: AS: 1.3: -103.8: 0: 9V:
    #381,S6,9V;
Indonesia: 28: 54: OC: -6.2: -106.8: 0: YB:
    #327,JZ,PK,PL,PM,PN,PO,YB,YC,YD,YE,YF,YG,YH,7A,7B,7C,7D,7E,7F,7G,7H,7I,8A,8B,8C,8D,8E,8F,8G,8H,8I;
Philippines: 27: 50: OC: 14.6: -121.0: 0: DU:
    #375,DU,DV,DW,DX,DY,DZ,4D,4E,4F,4G,4H,4I;
Timor-Leste: 28: 54: OC: -8.6: -125.6: 0: 4W:
    #511,4W;
Australia: 30: 55: OC: -33.9: -151.2: 0: VK:
    #150,AX,VH,VI,VJ,VK,VL,VM,VN,VZ,VK6(29)[58],VK8(29)[55],VK4(30)[55];
New Zealand: 32: 60: OC: -41.3: -174.8: 0: ZL:
    #170,ZK,ZL,ZM;
Fiji: 32: 56: OC: -18.1: -178.4: 0: 3D2:
    #176,3D2;
Tonga: 32: 62: OC: -21.1: 175.2: 0: A3:
    #160,A3;
Samoa: 32: 62: OC: -13.8: 171.8: 0: 5W:
    #190,5W;
Vanuatu: 32: 56: OC: -17.7: -168.3: 0: YJ:
    #158,YJ;
Papua New Guinea: 28: 51: OC: -9.4: -147.2: 0: P2:
    #163,P2;
Solomon Islands: 28: 51: OC: -9.4: -160.0: 0: H4:
    #185,H4;
New Caledonia: 32: 56: OC: -22.3: -166.5: 0: FK:
    #162,FK;
French Polynesia: 32: 63: OC: -17.5: 149.6: 0: FO:
    #175,FO;
Western Kiribati: 31: 65: OC: 1.4: -173.0: 0: T30:
    #301,T30;
Marshall Islands: 31: 65: OC: 7.1: -171.4: 0: V7:
    #168,V7;
Micronesia: 27: 65: OC: 6.9: -158.2: 0: V6:
    #173,V6;
Palau: 27: 64: OC: 7.5: -134.6: 0: T8:
    #22,T8;
South Africa: 38: 57: AF: -26.2: -28.0: 0: ZS:
    #462,H5,S8,V9,ZR,ZS,ZT,ZU;
Namibia: 38: 57: AF: -22.6: -17.1: 0: V5:
    #464,V5;
Botswana: 38: 57: AF: -24.7: -25.9: 0: A2:
    #402,A2,8O;
Zimbabwe: 38: 53: AF: -17.8: -31.0: 0: Z2:
    #452,Z2;
Zambia: 36: 53: AF: -15.4: -28.3: 0: 9J:
    #482,9I,9J;
Malawi: 37: 53: AF: -14.0: -33.8: 0: 7Q:
    #440,7Q;
Mozambique: 37: 53: AF: -25.9: -32.6: 0: C9:
    #181,C8,C9;
Madagascar: 39: 53: AF: -18.9: -47.5: 0: 5R:
    #438,5R,5S,6X;
Mauritius: 39: 53: AF: -20.2: -57.5: 0: 3B8:
    #165,3B8;
Reunion Island: 39: 53: AF: -21.1: -55.5: 0: FR:
    #453,FR;
Seychelles: 39: 53: AF: -4.6: -55.5: 0: S7:
    #379,S7;
Kenya: 37: 48: AF: -1.3: -36.8: 0: 5Z:
    #430,5Y,5Z;
Tanzania: 37: 53: AF: -6.8: -39.3: 0: 5H:
    #470,5H,5I;
Uganda: 37: 48: AF: 0.3: -32.6: 0: 5X:
    #286,5X;
Ethiopia: 37: 48: AF: 9.0: -38.7: 0: ET:
    #53,ET,9E,9F;
Sudan: 34: 48: AF: 15.6: -32.5: 0: ST:
    #466,ST,6T,6U;
Egypt: 34: 38: AF: 30.0: -31.2: 0: SU:
    #478,SS,SU,6A,6B;
Libya: 34: 38: AF: 32.9: -13.2: 0: 5A:
    #436,5A;
Tunisia: 33: 37: AF: 36.8: -10.2: 0: 3V:
    #474,TS,3V;
Algeria: 33: 37: AF: 36.8: -3.1: 0: 7X:
    #400,7R,7T,7U,7V,7W,7X,7Y;
Morocco: 33: 37: AF: 34.0: 6.8: 0: CN:
    #446,CN,5C,5D,5E,5F,5G;
Senegal: 35: 46: AF: 14.7: 17.4: 0: 6W:
    #456,6V,6W;
Cape Verde: 35: 46: AF: 14.9: 23.5: 0: D4:
    #409,D4;
Mali: 35: 46: AF: 12.6: 8.0: 0: TZ:
    #442,TZ;
Niger: 35: 46: AF: 13.5: -2.1: 0: 5U:
    #187,5U;
Cote d'Ivoire: 35: 46: AF: 5.3: 4.0: 0: TU:
    #428,TU;
Ghana: 35: 46: AF: 5.6: 0.2: 0: 9G:
    #424,9G;
Nigeria: 35: 46: AF: 9.1: -7.5: 0: 5N:
    #450,5N,5O;
Cameroon: 36: 47: AF: 3.9: -11.5: 0: TJ:
    #406,TJ;
Dem. Rep. of the Congo: 36: 52: AF: -4.3: -15.3: 0: 9Q:
    #414,9O,9P,9Q,9R,9S,9T;
Angola: 36: 52: AF: -8.8: -13.2: 0: D2:
    #401,D2,D3;
)CTY"
