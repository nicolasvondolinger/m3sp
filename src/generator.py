# python3 generator.py VRBSP 10 20 100

import sys
import secrets
import math
import os
import shutil

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

datas = [d for sublist in dataRates for d in sublist]

def euclidean(a, b, c, d):
    return math.sqrt((a - c) ** 2 + (b - d) ** 2)

def createInstance(filepath, TYPE, n, dim):
    alfa = 3.0
    noise = 0.0
    powerSender = 1000.0
    spectrums = [spec1, spec2, spec3]

    time_slots = 1 if TYPE == "VRBSP" else n
    if TYPE not in ["VRBSP", "MD-VRBSP"]:
        print("could not recognize problem type")
        sys.exit(1)

    with open(filepath, "w") as f:
        output = f"{n} {alfa} {noise} {powerSender} {qtd_spectrum} {' '.join(map(str, spectrums))}\n"
        f.write(output)

        receivers = [[secretsGenerator.uniform(0.0, dim) for _ in range(2)] for _ in range(n)]
        senders = [[]]

        sqrt_2 = math.sqrt(2)
        for elem in receivers:
            while True:
                c1 = secretsGenerator.uniform(0.0, dim)
                c2 = secretsGenerator.uniform(0.0, dim)
                if euclidean(elem[0], elem[1], c1, c2) <= (6 * sqrt_2):
                    break
            senders.append([c1, c2])

        f.write("\n" + "\n".join(" ".join(map(str, el)) for el in receivers) + "\n")
        f.write("\n" + "\n".join(" ".join(map(str, el)) for el in senders) + "\n")

        gamma = [secretsGenerator.choice(datas) for _ in range(n)]
        f.write("\n" + "\n".join(str(g) for g in gamma) + "\n\n")

        for row in dataRates:
            f.write(" ".join(map(str, row)) + "\n")
        f.write("\n")
        for row in SINR:
            f.write(" ".join(map(str, row)) + "\n")
        f.write("\n")

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python3 script.py <TYPE> <num_instances> <num_devices> <dimension>")
        sys.exit(1)

    TYPE = sys.argv[1]
    num_instances = int(sys.argv[2])
    num_devices = int(sys.argv[3])
    dimension = int(sys.argv[4])

    instance_dir = "instances"

    # Limpa e recria a pasta "instances"
    if os.path.exists(instance_dir):
        shutil.rmtree(instance_dir)
    os.makedirs(instance_dir)

    for i in range(num_instances):
        filename = f"instance_{i}.txt"
        filepath = os.path.join(instance_dir, filename)
        createInstance(filepath, TYPE, num_devices, dimension)

    print(f"{num_instances} instances generated in folder '{instance_dir}'.")