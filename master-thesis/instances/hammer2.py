import sys
import secrets

secretsGenerator = secrets.SystemRandom()

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


def adapt_instance(n, k):
    in_name = "./dr/U_" + str(n) + "/MD-VRBSP_U_" + n + "_" + k + ".txt"
    sgen = secrets.SystemRandom()

    with open(in_name, "r") as f_i:
        out_inst = []
        out_name = "./du/U_" + str(n) + "/MD-VRBSP_U_" + n + "_" + k + ".txt"

        for i, line in enumerate(f_i):
            if i >= (2 * int(n) + 4) and (i < 3 * int(n) + 4):
                gamma = str(datas[sgen.choice(range(0, len(datas)))])
                out_inst += str(gamma) + "\n"
                continue

            out_inst += [line]

        with open(out_name, "w") as f_o:
            f_o.writelines(out_inst)
        #     f_o.write("\n".join(out_inst))


if __name__ == "__main__":
    adapt_instance(sys.argv[1], sys.argv[2])
