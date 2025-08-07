#!/usr/bin/env python3

import sys
import secrets
import math

secretsGenerator = secrets.SystemRandom()

qtd_spectrum, spec1, spec2, spec3 = 3, 160, 240, 100

SINR = [
    [2, 5, 8, 11],
    [5, 8, 11, 14],
    [7, 10, 13, 16],
    [10, 13, 16, 19],
    [14, 17, 20, 23],
    [18, 21, 24, 27],
    [19, 22, 25, 28],
    [20, 23, 26, 29],
    [25, 19, 31, 34],
    [27, 30, 33, 36],
    [30, 33, 36, 39],
    [32, 35, 38, 41],
]


dataRates = [
    [8.6, 17.2, 36.0, 72.1],
    [17.2, 34.4, 72.1, 144.1],
    [25.8, 51.6, 108.1, 216.2],
    [34.4, 68.8, 144.1, 288.2],
    [51.6, 103.2, 216.2, 432.4],
    [68.8, 137.6, 288.2, 576.5],
    [77.4, 154.9, 324.3, 648.5],
    [86.0, 172.1, 360.3, 720.6],
    [103.2, 206.5, 432.4, 864.7],
    [114.7, 229.4, 480.4, 960.8],
    [129.0, 258.1, 540.4, 1080.9],
    [143.4, 286.8, 600.5, 1201.0],
]

dataRates_T = [
    [8.6, 17.2, 25.8, 34.4, 51.6, 68.8, 77.4, 86.0, 103.2, 114.7, 129.0, 143.4],
    [17.2, 34.4, 51.6, 68.8, 103.2, 137.6, 154.9, 172.1, 206.5, 229.4, 258.1, 286.8],
    [36.0, 72.1, 108.1, 144.1, 216.2, 288.2, 324.3, 360.3, 432.4, 480.4, 540.4, 600.5],
    [
        72.1,
        144.1,
        216.2,
        288.2,
        432.4,
        576.5,
        648.5,
        720.6,
        864.7,
        960.8,
        1080.9,
        1201.0,
    ],
]


bw_idx = [0, 1, 2, 3]

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
    72.1,
    144.1,
    216.2,
    288.2,
    432.4,
    576.5,
    648.5,
    720.6,
    864.7,
    960.8,
    1080.9,
    1201,
]


def euclidean(a, b, c, d):
    return math.sqrt(math.pow(a - c, 2) + math.pow(b - d, 2))


def createInstance(TYPE, inst, n, dim):
    global qtd_spectrum, spec1, spec2, spec3
    filename = TYPE
    filename += "_U_" + str(n) + "_" + str(inst) + ".txt"
    # filename = "./inst_"
    # filename += str(inst)
    # filename += ".txt"
    f = open(filename, "w")

    alfa = 3.0
    noise = 0.0
    powerSender = 1000.0
    spectrums = [spec1, spec2, spec3]

    time_slots = n
    if TYPE == "VRBSP":
        time_slots = 1
    elif TYPE != "MD-VRBSP":
        print("could not recognize problem type (time_slots)")
        sys.exit(1)

    output = str(n) + " " + str(alfa) + " " + str(noise) + " " + str(powerSender) + " "

    output += str(qtd_spectrum)
    if TYPE != "VRBSP" and TYPE != "MD-VRBSP":
        print("could not recognize problem type")
        sys.exit(1)

    for i in range(qtd_spectrum):
        output += " " + str(spectrums[i])
    output += "\n"

    f.write(output)

    receivers = [
        [secretsGenerator.uniform(0.0, dim) for _ in range(2)] for _ in range(n)
    ]
    senders = [[]]

    sqrt_2 = math.sqrt(2)
    for elem in receivers:
        c1 = 0.0
        c2 = 0.0
        while True:
            c1 = secretsGenerator.uniform(0.0, float(dim))
            c2 = secretsGenerator.uniform(0.0, float(dim))
            if euclidean(elem[0], elem[1], c1, c2) <= (6 * sqrt_2):
                break

        senders.append([c1, c2])

    f.write("\n")
    f.write(
        "\n".join([" ".join([str(el[j]) for j in range(len(el))]) for el in receivers])
    )

    f.write("\n")
    f.write(
        "\n".join([" ".join([str(el[j]) for j in range(len(el))]) for el in senders])
    )

    f.write("\n")

    gamma = [secretsGenerator.choice(datas) for _ in range(n)]

    f.write("\n")
    f.write("\n".join(str(gamma[idx]) for idx in range(n)))

    f.write("\n\n")
    for i in range(len(dataRates)):
        for j in range(len(dataRates[i]) - 1):
            to_out = str(dataRates[i][j]) + " "
            f.write(to_out)

        f.write(str(dataRates[i][len(dataRates[i]) - 1]))
        f.write("\n")

    f.write("\n")
    for i in range(len(SINR)):
        for j in range(len(SINR[i]) - 1):
            to_out = str(SINR[i][j]) + " "
            f.write(to_out)

        f.write(str(SINR[i][len(SINR[i]) - 1]))
        f.write("\n")

    f.write("\n")
    f.close()


if __name__ == "__main__":
    # TYPE, number of instances, number of devices, dimension
    if len(sys.argv) != 5:
        print("wrong arguments")
        exit

    for i in range(int(sys.argv[2])):
        createInstance(sys.argv[1], i + 1, int(sys.argv[3]), int(sys.argv[4]))
