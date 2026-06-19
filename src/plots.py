#!/usr/bin/env python3
# =============================================================================
#  plots.py - genera las figuras del informe a partir de los CSV de resultados/
#  y del JSON de la instancia real. Salida: figs/*.png (incluidas con
#  \includegraphics en main.tex).
#
#  USO:  python plots.py        (parado en src/ o en cualquier lado)
#  REQUIERE: matplotlib, numpy.
#
#  Cada figura se genera de forma independiente: si falta un CSV, esa figura se
#  saltea con un aviso y las demas se generan igual.
# =============================================================================

import os
import csv
import json
import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "resultados")
FIGS = os.path.join(HERE, "..", "figs")
JSON = os.path.join(HERE, "instances", "real", "real_instance.json")
SALIDA = os.path.join(HERE, "solucion_competencia.txt")  # solucion competencia: una linea por deposito
os.makedirs(FIGS, exist_ok=True)

# dimensiones (m depositos, n vendedores) por instancia, para el tamano n*m
DIMS = {
    "a05100": (5, 100), "a05200": (5, 200), "a10100": (10, 100), "a10200": (10, 200),
    "a20100": (20, 100), "a20200": (20, 200),
    "b05100": (5, 100), "b05200": (5, 200), "b10100": (10, 100), "b10200": (10, 200),
    "b20100": (20, 100), "b20200": (20, 200),
    "e05100": (5, 100), "e05200": (5, 200), "e10100": (10, 100), "e10200": (10, 200),
    "e10400": (10, 400), "e20100": (20, 100), "e20200": (20, 200), "e20400": (20, 400),
    "e40400": (40, 400), "e15900": (15, 900), "e30900": (30, 900), "e60900": (60, 900),
    "e201600": (20, 1600), "e401600": (40, 1600), "e801600": (80, 1600),
    "real": (310, 1100),
}

# paleta de metodos
COL = {
    "Constructiva": "#9e9e9e",
    "VND": "#1f77b4",
    "ILS": "#ff7f0e",
    "LNS": "#2ca02c",
}


def leer_csv(nombre):
    path = os.path.join(RES, nombre)
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def guardar(fig, nombre, dpi=150):
    out = os.path.join(FIGS, nombre)
    fig.tight_layout()
    fig.savefig(out, dpi=dpi, bbox_inches="tight")
    plt.close(fig)
    print("  ok ->", os.path.relpath(out, HERE))


# ---------------------------------------------------------------------------
#  fig:gap  -  gap medio (%) por metodo, por grupo a/b/e + real
# ---------------------------------------------------------------------------
def fig_gap():
    rows = leer_csv("e5_final.csv")
    metodos = [("Constructiva", "gap_constructiva_pct"), ("VND", "gap_vnd_pct"),
               ("ILS", "gap_ils_pct"), ("LNS", "gap_lns_pct")]
    metodos = [(m, c) for (m, c) in metodos if c in rows[0]]  # LNS solo si existe
    cats = ["a", "b", "e", "real"]

    def prom(grupo, col):
        vals = [float(r[col]) for r in rows if r["grupo"] == grupo]
        return sum(vals) / len(vals) if vals else 0.0

    data = {m: [prom("a", c), prom("b", c), prom("e", c), prom("r", c)]
            for (m, c) in metodos}

    fig, ax = plt.subplots(figsize=(7, 4))
    x = np.arange(len(cats))
    w = 0.8 / len(metodos)
    for i, (m, _) in enumerate(metodos):
        ax.bar(x + i * w - 0.4 + w / 2, data[m], w, label=m, color=COL.get(m))
    ax.set_xticks(x); ax.set_xticklabels(["Grupo a", "Grupo b", "Grupo e", "Real"])
    ax.set_ylabel("Gap medio (%) vs mejor encontrado")
    ax.set_title("Calidad por método (gap respecto del mejor valor por instancia)")
    ax.legend(); ax.grid(axis="y", alpha=0.3)
    guardar(fig, "fig_gap_metodo.png")


# ---------------------------------------------------------------------------
#  fig:tiempo  -  tiempo de VND vs tamano n*m (log-log)
# ---------------------------------------------------------------------------
def fig_tiempo():
    rows = leer_csv("e3_vnd.csv")
    gcolor = {"a": "#1f77b4", "b": "#ff7f0e", "e": "#2ca02c", "r": "#d62728"}
    xs, ys, cs = [], [], []
    for r in rows:
        if r["metodo"] != "vnd_completo":
            continue
        nm = r["instancia"]
        if nm not in DIMS:
            continue
        m, n = DIMS[nm]
        t = float(r["tiempo_s"])
        if t <= 0:
            t = 1e-4  # piso para escala log
        xs.append(m * n); ys.append(t); cs.append(gcolor.get(r["grupo"], "#333"))

    fig, ax = plt.subplots(figsize=(6.5, 4))
    ax.scatter(xs, ys, c=cs, s=40, edgecolors="k", linewidths=0.3)
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlabel(r"Tamaño $n\cdot m$"); ax.set_ylabel("Tiempo de VND (s)")
    ax.set_title("Escalabilidad de la búsqueda local (VND completo)")
    ax.grid(True, which="both", alpha=0.3)
    handles = [plt.Line2D([], [], marker="o", ls="", color=gcolor[g],
                          label={"a": "a", "b": "b", "e": "e", "r": "real"}[g])
               for g in ["a", "b", "e", "r"]]
    ax.legend(handles=handles, title="Grupo")
    guardar(fig, "fig_tiempo_tamano.png")


# ---------------------------------------------------------------------------
#  fig:conv  -  costo del incumbente vs tiempo (ILS y LNS) en e20200 y real
# ---------------------------------------------------------------------------
def fig_convergencia():
    rows = leer_csv("convergencia.csv")
    insts = []
    for r in rows:
        if r["instancia"] not in insts:
            insts.append(r["instancia"])

    fig, axes = plt.subplots(1, len(insts), figsize=(5 * len(insts), 4))
    if len(insts) == 1:
        axes = [axes]
    for ax, inst in zip(axes, insts):
        for met, col in (("ILS", COL["ILS"]), ("LNS", COL["LNS"])):
            pts = [(float(r["segundos"]), float(r["costo"])) for r in rows
                   if r["instancia"] == inst and r["metodo"] == met]
            pts.sort()
            if pts:
                xs, ys = zip(*pts)
                ax.plot(xs, ys, marker="o", label=met, color=col)
        ax.set_title(f"Convergencia — {inst}")
        ax.set_xlabel("Presupuesto de tiempo (s)")
        ax.set_ylabel("Costo del incumbente")
        ax.legend(); ax.grid(alpha=0.3)
    guardar(fig, "fig_convergencia.png")


# ---------------------------------------------------------------------------
#  fig:mapa  -  layout geografico de la instancia real (vendedores / depositos)
# ---------------------------------------------------------------------------
def fig_mapa():
    with open(JSON) as f:
        inst = json.load(f)["instance"]
    cli = np.array(inst["client_position"])
    dep = np.array(inst["depot_position"])
    fig, ax = plt.subplots(figsize=(6.2, 6))

    # lineas vendedor->deposito segun la solucion de competencia: la linea i
    # (0-indexada) lista los ids de vendedores asignados al deposito i. Indices
    # alineados con los arrays del JSON. Se dibujan debajo de los puntos.
    segmentos = []
    if os.path.exists(SALIDA):
        with open(SALIDA) as f:
            for i, linea in enumerate(f):
                if i >= len(dep):
                    break
                for tok in linea.split():
                    j = int(tok)
                    if 0 <= j < len(cli):
                        segmentos.append([cli[j], dep[i]])
        if segmentos:
            ax.add_collection(LineCollection(
                segmentos, colors="black", linewidths=0.3, alpha=0.4, zorder=1))
            ax.plot([], [], color="black", lw=0.8,
                    label=f"Asignaciones ({len(segmentos)})")
    else:
        print("  -- fig:mapa: falta solucion_competencia.txt, mapa sin lineas de asignación")

    ax.scatter(cli[:, 0], cli[:, 1], s=6, c="#d62728", alpha=0.5, zorder=2,
               label=f"Vendedores (n={len(cli)})")
    ax.scatter(dep[:, 0], dep[:, 1], s=26, c="#1f77b4", edgecolors="k",
               linewidths=0.3, zorder=3, label=f"Depósitos (m={len(dep)})")
    ax.set_xlabel(""); ax.set_ylabel("")
    ax.set_xticks([]); ax.set_yticks([])
    ax.set_title("Solución de la instancia real")
    ax.set_aspect("equal", adjustable="datalim")
    ax.legend(loc="upper right")
    guardar(fig, "fig_mapa_real.png", dpi=600)


# ---------------------------------------------------------------------------
#  extra  -  mejora (%) por operador de busqueda local (relocate/swap x first/best)
# ---------------------------------------------------------------------------
def fig_bl_operadores():
    rows = leer_csv("e2_busqueda_local.csv")
    ops = ["relocate_first", "relocate_best", "swap_first", "swap_best"]
    media = {o: [] for o in ops}
    for r in rows:
        if r["operador"] in media:
            media[r["operador"]].append(float(r["mejora_pct"]))
    prom = [np.mean(media[o]) if media[o] else 0 for o in ops]
    mx = [np.max(media[o]) if media[o] else 0 for o in ops]

    fig, ax = plt.subplots(figsize=(6.5, 4))
    x = np.arange(len(ops))
    ax.bar(x - 0.2, prom, 0.4, label="Mejora media", color="#1f77b4")
    ax.bar(x + 0.2, mx, 0.4, label="Mejora máx", color="#aec7e8")
    ax.set_xticks(x)
    ax.set_xticklabels(["relocate\nfirst", "relocate\nbest", "swap\nfirst", "swap\nbest"])
    ax.set_ylabel("Mejora sobre la constructiva (%)")
    ax.set_title("Aporte de cada operador de búsqueda local")
    ax.legend(); ax.grid(axis="y", alpha=0.3)
    guardar(fig, "fig_bl_operadores.png")


# ---------------------------------------------------------------------------
#  extra  -  ILS vs LNS en las instancias de tuning (costo medio, normalizado)
# ---------------------------------------------------------------------------
def fig_tuning():
    rows = leer_csv("e4_tuning.csv")
    insts = []
    for r in rows:
        if r["instancia"] not in insts:
            insts.append(r["instancia"])
    # mejor costo medio por (instancia, metaheuristica)
    best = {}
    for r in rows:
        key = (r["instancia"], r["metaheuristica"])
        c = float(r["costo_medio"])
        best[key] = min(best.get(key, math.inf), c)

    ils = [best.get((i, "ILS"), math.nan) for i in insts]
    lns = [best.get((i, "LNS"), math.nan) for i in insts]
    # normalizar a ILS=1 para poder comparar en un mismo eje
    ilsn = [1.0 for _ in insts]
    lnsn = [lns[k] / ils[k] if ils[k] else math.nan for k in range(len(insts))]

    fig, ax = plt.subplots(figsize=(6.5, 4))
    x = np.arange(len(insts))
    ax.bar(x - 0.2, ilsn, 0.4, label="ILS (mejor config)", color=COL["ILS"])
    ax.bar(x + 0.2, lnsn, 0.4, label="LNS (mejor config)", color=COL["LNS"])
    ax.axhline(1.0, color="k", lw=0.8, ls="--")
    ax.set_xticks(x); ax.set_xticklabels(insts)
    ax.set_ylabel("Costo medio relativo a ILS")
    ax.set_title("Tuning: ILS vs LNS (menor es mejor)")
    ax.legend(); ax.grid(axis="y", alpha=0.3)
    guardar(fig, "fig_tuning.png")


FIGURAS = [
    ("fig:gap", fig_gap),
    ("fig:tiempo", fig_tiempo),
    ("fig:conv", fig_convergencia),
    ("fig:mapa", fig_mapa),
    ("fig_bl_operadores", fig_bl_operadores),
    ("fig_tuning", fig_tuning),
]

if __name__ == "__main__":
    print("Generando figuras en", os.path.relpath(FIGS, HERE))
    for nombre, fn in FIGURAS:
        try:
            fn()
        except FileNotFoundError as e:
            print(f"  -- {nombre}: falta CSV ({os.path.basename(str(e))}), se saltea")
        except Exception as e:
            print(f"  !! {nombre}: error: {e}")
