#!/usr/bin/env python3
"""
Gera UM gráfico de barras agrupadas com a média de canais de 20/40/80/160 MHz
presentes na solução final, comparando várias heurísticas (uma por diretório).

Cada diretório de resultado (ex.: output/output_fixed_dp_5/1024) deve conter um
arquivo channels.txt, escrito pelo C++, com uma linha por instância no formato:

    c20 c40 c80 c160

onde cada valor é a contagem de canais daquela largura de banda na solução final
daquela instância. A altura de cada barra é a média dessas contagens ao longo das
instâncias do grupo.

Uso:
    python plot_channels.py DIR [DIR ...] [--labels "CH,VNS,BRKGA"]
                                          [--output arquivo.png]
                                          [--title "..."] [--show]

Exemplo:
    python plot_channels.py \
        output/output_fixed_dp_5/1024 \
        output/output_local_search_5/1024 \
        output/output_new_heuristic_5/1024 \
        --labels "Fixed DP,BRKGA+LS,New Heuristic"

Regras:
    - Todos os diretórios devem ser do MESMO tamanho de instância (último
      componente do path, ex.: 1024); caso contrário o script aborta.
    - Independentemente de quantos diretórios forem passados, é gerado apenas
      um único gráfico.
"""

import argparse
import os
import re
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")  # backend headless; savefig funciona sem display
import matplotlib.pyplot as plt

# Larguras de banda (colunas de channels.txt, nesta ordem) e eixo X do gráfico.
BANDWIDTHS = [20, 40, 80, 160]

# Rótulos amigáveis por nome de pasta (após remover "output_" e o sufixo "_<pe>").
LABEL_MAP = {
    "": "Classic",
    "dp": "DP",
    "fixed_dp": "Fixed DP",
    "random_dp": "Random DP",
    "new_heuristic": "New Heuristic",
    "local_search": "BRKGA+LS",
}


def instance_size(path):
    """Último componente do path, ex.: output/output_fixed_dp_5/1024 -> '1024'."""
    return os.path.basename(os.path.normpath(path))


def derive_label(path):
    """Deriva um rótulo legível a partir do nome da pasta pai do diretório."""
    parent = os.path.basename(os.path.dirname(os.path.normpath(path)))
    core = parent
    if core.startswith("output_"):
        core = core[len("output_"):]
    core = re.sub(r"_?\d+$", "", core)  # remove o sufixo do pe (ex.: _5)
    if core in LABEL_MAP:
        return LABEL_MAP[core]
    return core.replace("_", " ").title() if core else parent


def load_means(path):
    """Lê channels.txt e devolve a média (por largura de banda) sobre as instâncias."""
    channels_file = os.path.join(path, "channels.txt")
    if not os.path.isfile(channels_file):
        sys.exit(f"[erro] arquivo não encontrado: {channels_file}\n"
                 f"       Rode os experimentos com o C++ atualizado para gerá-lo.")

    data = np.loadtxt(channels_file, ndmin=2)
    if data.size == 0:
        sys.exit(f"[erro] arquivo vazio: {channels_file}")
    if data.shape[1] != len(BANDWIDTHS):
        sys.exit(f"[erro] {channels_file}: esperado {len(BANDWIDTHS)} colunas "
                 f"(20 40 80 160), encontrado {data.shape[1]}.")

    return data.mean(axis=0), data.shape[0]


def main():
    parser = argparse.ArgumentParser(
        description="Gráfico da média de canais por largura de banda (compara heurísticas).")
    parser.add_argument("dirs", nargs="+",
                        help="Diretórios de resultado a comparar (ex.: output/output_fixed_dp_5/1024).")
    parser.add_argument("--labels", default=None,
                        help="Rótulos das heurísticas, separados por vírgula, na ordem dos diretórios.")
    parser.add_argument("--output", default=None,
                        help="Arquivo de saída do gráfico (default: channels_<tamanho>.png).")
    parser.add_argument("--title", default=None,
                        help="Título do gráfico (default: |L| = <tamanho>).")
    parser.add_argument("--show", action="store_true",
                        help="Também exibe o gráfico numa janela (requer display).")
    args = parser.parse_args()

    # (1) Validação: todos os diretórios devem ser do mesmo tamanho de instância.
    sizes = {instance_size(d) for d in args.dirs}
    if len(sizes) != 1:
        sys.exit(f"[erro] todos os diretórios devem ter o mesmo tamanho de instância; "
                 f"encontrados: {sorted(sizes)}")
    size = sizes.pop()

    # (2) Rótulos: explícitos (--labels) ou derivados do nome da pasta.
    if args.labels is not None:
        labels = [s.strip() for s in args.labels.split(",")]
        if len(labels) != len(args.dirs):
            sys.exit(f"[erro] --labels tem {len(labels)} rótulos, mas foram passados "
                     f"{len(args.dirs)} diretórios.")
    else:
        labels = [derive_label(d) for d in args.dirs]

    # (3) Média por largura de banda para cada heurística.
    means = []
    for d, label in zip(args.dirs, labels):
        m, n_inst = load_means(d)
        means.append(m)
        print(f"{label:>16}: média sobre {n_inst} instâncias -> "
              + ", ".join(f"{b}MHz={v:.2f}" for b, v in zip(BANDWIDTHS, m)))

    # (4) Um único gráfico de barras agrupadas.
    x = np.arange(len(BANDWIDTHS))
    n_algos = len(args.dirs)
    total_width = 0.8
    bar_width = total_width / n_algos

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for k, (label, m) in enumerate(zip(labels, means)):
        offset = (k - (n_algos - 1) / 2.0) * bar_width
        ax.bar(x + offset, m, bar_width, label=label)

    ax.set_xlabel("Bandwidth Value")
    ax.set_ylabel("No. of occurrences")
    ax.set_title(args.title if args.title else f"|L| = {size}")
    ax.set_xticks(x)
    ax.set_xticklabels(BANDWIDTHS)
    ax.legend(title="Algorithm")
    ax.margins(y=0.05)
    fig.tight_layout()

    output = args.output if args.output else f"channels_{size}.png"
    fig.savefig(output, dpi=150, bbox_inches="tight")
    print(f"\nGráfico salvo em: {output}")

    if args.show:
        try:
            plt.show()
        except Exception as exc:  # pragma: no cover
            print(f"[aviso] não foi possível exibir a janela: {exc}")


if __name__ == "__main__":
    main()
