#!/usr/bin/env python3
"""
Le o log plano (flat profile) do gprof e gera graficos de analise de desempenho:

  - calls.png       : numero de chamadas por funcao
  - total_time.png  : tempo total (self) gasto em cada funcao
  - per_call.png    : tempo medio por chamada (self / chamadas)

Por padrao foca nas funcoes do PROJETO (as internas da STL/compilador sao
somadas numa unica barra "STL/lib"). Use --all para incluir tudo separado.

As imagens sao salvas na mesma pasta do log (por padrao, profile/).

Uso:
    python plot_profile.py [profile/gprof.txt] [--top N] [--all] [--outdir DIR]

O log e gerado por:
    cd src && make profile          # compila com -pg, roda e chama este script
"""

import argparse
import os
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Uma linha do flat profile do gprof:
#  %time  cumulative  self  [calls  self/call  total/call]  name
# O tempo por chamada e calculado por nos (self_seconds / calls) para ter
# unidade consistente (as colunas do gprof mudam de s/ms/us conforme o valor).
ROW_RE = re.compile(
    r"^\s*([\d.]+)\s+[\d.]+\s+([\d.]+)(?:\s+(\d+)\s+[\d.]+\s+[\d.]+)?\s+(.+?)\s*$"
)

# Marcadores que identificam funcoes internas da STL / runtime do compilador.
LIB_MARKERS = (
    "std::", "__gnu_cxx", "__cxx", "_M_", "_S_", "_Vector_base", "_Construct",
    "_Destroy", "__do_", "forward<", "operator new", "operator delete",
    "_Rb_tree", "_Hashtable", "__detail", "__ostream", "__istream",
)


class Func:
    __slots__ = ("name", "pct", "self_s", "calls")

    def __init__(self, name, pct, self_s, calls):
        self.name = name
        self.pct = pct
        self.self_s = self_s
        self.calls = calls

    @property
    def per_call_us(self):
        if not self.calls:
            return None
        return self.self_s / self.calls * 1e6


def head_of(name):
    return name.split("(")[0]


def is_library(name):
    return any(m in head_of(name) for m in LIB_MARKERS)


def clean_name(name):
    """Nome curto e legivel: remove args, templates <...> e o tipo de retorno."""
    head = head_of(name).strip()
    out, depth = [], 0
    for ch in head:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(ch)
    s = "".join(out).strip()
    if " " in s:                      # descarta tipo de retorno (ex.: "void x::y")
        s = s.split()[-1]
    return s or head


def parse_gprof(path):
    if not os.path.isfile(path):
        sys.exit(f"[erro] log do gprof nao encontrado: {path}\n"
                 f"       Gere-o com: cd src && make profile")
    funcs, started = [], False
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if not started:
                low = line.lower()
                if "time" in low and "seconds" in low and "calls" in low:
                    started = True
                continue
            if line.strip() == "" or line.startswith("\x0c") \
                    or "Call graph" in line or "granularity" in line:
                break
            m = ROW_RE.match(line)
            if not m:
                continue
            calls = int(m.group(3)) if m.group(3) is not None else None
            funcs.append(Func(m.group(4).strip(), float(m.group(1)),
                              float(m.group(2)), calls))
    if not funcs:
        sys.exit(f"[erro] nenhuma funcao encontrada no flat profile de {path}.")
    return funcs


def barh(labels, values, annots, title, xlabel, color, outfile, logx=False, subtitle=None):
    n = len(labels)
    fig, ax = plt.subplots(figsize=(10, max(2.5, 0.5 * n + 1.3)))
    y = list(range(n))
    ax.barh(y, values, color=color, edgecolor="none")
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=9)
    ax.invert_yaxis()
    if logx:
        ax.set_xscale("log")
    ax.set_xlabel(xlabel)
    if subtitle:
        fig.suptitle(title, fontsize=13, fontweight="bold")
        ax.set_title(subtitle, fontsize=8.5, color="#555555", pad=6)
    else:
        ax.set_title(title, fontsize=13, fontweight="bold")
    ax.grid(axis="x", linestyle=":", alpha=0.5)
    for yi, (v, a) in enumerate(zip(values, annots)):
        ax.text(v, yi, "  " + a, va="center", ha="left", fontsize=8)
    ax.margins(x=0.16)
    fig.subplots_adjust(left=0.28, right=0.97, top=0.9, bottom=0.12)
    fig.savefig(outfile, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  -> {outfile}")


def human_int(v):
    return f"{v:,}".replace(",", ".")


def main():
    ap = argparse.ArgumentParser(description="Graficos de profiling a partir do log do gprof.")
    ap.add_argument("gprof", nargs="?", default="profile/gprof.txt",
                    help="log plano do gprof (default: profile/gprof.txt)")
    ap.add_argument("--top", type=int, default=15, help="funcoes por grafico (default: 15)")
    ap.add_argument("--all", action="store_true",
                    help="inclui funcoes internas da STL separadas (default: agregadas)")
    ap.add_argument("--outdir", default=None, help="pasta de saida (default: pasta do log)")
    args = ap.parse_args()

    funcs = parse_gprof(args.gprof)
    outdir = args.outdir or (os.path.dirname(args.gprof) or ".")
    os.makedirs(outdir, exist_ok=True)

    total_self = sum(f.self_s for f in funcs)
    if args.all:
        project, lib = funcs, []
    else:
        project = [f for f in funcs if not is_library(f.name)]
        lib = [f for f in funcs if is_library(f.name)]
    lib_self = sum(f.self_s for f in lib)
    lib_calls = sum(f.calls for f in lib if f.calls)

    print(f"Funcoes lidas: {len(funcs)} | tempo total (self): {total_self:.3f}s")
    if lib:
        print(f"Internas STL/lib agregadas: {len(lib)} funcoes, "
              f"{lib_self:.3f}s ({100*lib_self/total_self:.1f}%)")
    print(f"Salvando graficos em: {outdir}/\n")
    sub = None if args.all else f"funcoes do projeto (STL/lib agregada); tempo total self = {total_self:.1f}s"

    # 1) Numero de chamadas
    with_calls = [f for f in project if f.calls]
    top = sorted(with_calls, key=lambda f: f.calls, reverse=True)[:args.top]
    labels = [clean_name(f.name) for f in top]
    vals = [f.calls for f in top]
    annots = [human_int(f.calls) for f in top]
    if lib and lib_calls:
        labels.append("«STL/lib (agregado)»"); vals.append(lib_calls); annots.append(human_int(lib_calls))
    if labels:
        barh(labels, vals, annots, f"Numero de chamadas por funcao (top {len(top)})",
             "chamadas (escala log)", "#4C72B0",
             os.path.join(outdir, "calls.png"), logx=True, subtitle=sub)

    # 2) Tempo total (self)
    top = [f for f in sorted(project, key=lambda f: f.self_s, reverse=True) if f.self_s > 0][:args.top]
    labels = [clean_name(f.name) for f in top]
    vals = [f.self_s for f in top]
    annots = [f"{f.self_s:.2f}s ({f.pct:.1f}%)" for f in top]
    if lib and lib_self > 0:
        labels.append("«STL/lib (agregado)»"); vals.append(lib_self)
        annots.append(f"{lib_self:.2f}s ({100*lib_self/total_self:.1f}%)")
    if labels:
        barh(labels, vals, annots, f"Tempo total (self) por funcao (top {len(top)})",
             "segundos", "#C44E52", os.path.join(outdir, "total_time.png"), subtitle=sub)

    # 3) Tempo medio por chamada (self / chamadas) - so funcoes do projeto
    per = [f for f in with_calls if f.per_call_us and f.per_call_us > 0]
    top = sorted(per, key=lambda f: f.per_call_us, reverse=True)[:args.top]
    if top:
        barh([clean_name(f.name) for f in top], [f.per_call_us for f in top],
             [f"{f.per_call_us:.2f} us" for f in top],
             f"Tempo medio por chamada - self (top {len(top)})",
             "microssegundos por chamada", "#55A868",
             os.path.join(outdir, "per_call.png"), subtitle=sub)

    # Resumo em texto
    print("\nTop funcoes do projeto por tempo total (self):")
    print(f"  {'funcao':<26} {'%':>6} {'self(s)':>9} {'chamadas':>16} {'us/chamada':>11}")
    for f in sorted(project, key=lambda f: f.self_s, reverse=True)[:args.top]:
        pc = f"{f.per_call_us:.2f}" if f.per_call_us is not None else "-"
        calls = human_int(f.calls) if f.calls else "-"
        print(f"  {clean_name(f.name)[:25]:<26} {f.pct:>6.1f} {f.self_s:>9.3f} {calls:>16} {pc:>11}")


if __name__ == "__main__":
    main()
