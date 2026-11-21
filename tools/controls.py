# Control positions in the panel artwork (source pixels, 1308 x 1203).
# type: fluted | chicken | bar | smallchick | toggle | screw
CONTROLS = {
    # EQ
    "lfBoost":  ("fluted", 217, 142), "lfAtten": ("fluted", 397, 143), "midAmt": ("fluted", 588, 143),
    "hfBoost":  ("fluted", 776, 143), "hfAtten": ("fluted", 965, 143), "bandwidth": ("fluted", 757, 345),
    "attenSel": ("chicken_s", 1128, 132), "lowCut": ("chicken", 165, 345), "lfFreq": ("chicken", 335, 345),
    "midFreq":  ("bar", 513, 345), "hfFreq": ("chicken", 925, 345), "highCut": ("chicken", 1110, 345),
    "eqIn": ("toggle", 118, 228), "tube": ("toggle", 1170, 216), "midBoost": ("toggle", 627, 347),
    # compressor
    "inGain1": ("fluted", 697, 598), "inGain2": ("fluted", 697, 866),
    "thresh1": ("fluted", 928, 598), "thresh2": ("fluted", 928, 866),
    "meter1": ("meterknob", 486, 600), "meter2": ("meterknob", 486, 868),
    "tc1": ("chicken_s", 1140, 570), "tc2": ("chicken_s", 1140, 918), "agcMode": ("chicken_s", 1150, 725),
    "scFilter1": ("chicken_s", 170, 1090), "scFilter2": ("chicken_s", 300, 1088),
    "dcThr1": ("smallchick", 413, 1090), "dcThr2": ("smallchick", 560, 1090),
    "mix1": ("smallchick", 752, 1090), "mix2": ("smallchick", 898, 1090),
    "output1": ("smallchick", 1027, 1090), "output2": ("smallchick", 1170, 1090),
    "power": ("toggle", 130, 615), "chLink": ("toggle", 130, 885), "compIn": ("toggle", 657, 1092),
    "zero1": ("screw", 428, 690), "bal1": ("screw", 546, 690), "zero2": ("screw", 428, 960), "bal2": ("screw", 546, 960),
}
# radius of the area that belongs to the control (incl. its drop shadow)
RADIUS = {"fluted": 74, "chicken": 50, "chicken_s": 52, "bar": 52, "smallchick": 42, "meterknob": 100, "toggle": 30, "screw": 12}
