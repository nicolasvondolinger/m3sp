#! /usr/bin/env python3

import os
import sys
import secrets
from itertools import product

U = [x for x in os.listdir("./") if os.path.isdir(x)]

datas = [
    8.6,
    17.2,
    25.8,
    34.4,
    51.6,
    68.8,
    77.4,
    86.0,
    103.2,
    114.7,
    129.0,
    143.4,
    17.2,
    34.4,
    51.6,
    68.8,
    103.2,
    137.6,
    154.9,
    172.1,
    206.5,
    229.4,
    258.1,
    286.8,
    36.0,
    72.1,
    108.1,
    144.1,
    216.2,
    288.2,
    324.3,
    260.3,
    432.4,
    480.4,
    540.4,
    600.5,
]

for u in U:
    print(u[2:])
    inst = -1
    L = [8, 16, 32, 64, 128, 256, 512, 1024, 2048]
    for i in L:
        if str(i) == u[2:]:
            inst = i
            break

    if inst == -1:
        continue
    
    for i in range(30):
        path = u + "/MD-VRBSP_" + u + "_" + str(i + 1) + ".txt"
        print("opening " + path)
        sgen = secrets.SystemRandom()
        lines = []
        with open(path, "r") as fl:
            lines = fl.readlines()
            for j in range(inst):
                lines[inst * 2 + 4 + j] = (
                    str(datas[sgen.choice(range(0, len(datas)))]) + "\n"
                )

        path2 = u + '/' + u + '_' + str(i + 1) + '.txt'
        print('writing in ' + path2)
        with open(path2, 'w') as fl:
            fl.writelines(lines)
