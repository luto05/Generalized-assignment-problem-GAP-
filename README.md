# Problema de Asignación Generalizada (GAP)

Resuelve el **Problema de Asignación Generalizada (GAP)** aplicado a la logística de
primera milla de la startup *ThunderPack*: asignar cada **vendedor** a exactamente un
**depósito**, respetando la capacidad de cada depósito y **minimizando la distancia total**
que recorren los vendedores. Si la capacidad no alcanza para todos, se admiten soluciones
parciales penalizando a cada vendedor sin asignar.

El código está escrito en **C++** e implementa dos heurísticas constructivas (vecino más
cercano e inserción por arrepentimiento), operadores de búsqueda local encadenados en un VND,
y dos metaheurísticas que lo usan internamente (ILS y LNS). En `src/plots.py` se generan las
figuras del informe.

## Requisitos

- `g++` con soporte de C++17.
- Para las figuras: `python3` con `matplotlib` y `numpy`.

## Ejecución

Todos los comandos se corren parados en `src/` (las rutas a las instancias son relativas).

### Resolver una instancia

```bash
cd src
make                                            # compila -> ./gap_simulator
./gap_simulator <instancia> <salida> [segundos]
```

- `<instancia>`: archivo de entrada en formato OR-Library (p. ej. `instances/real/real_instance`).
- `<salida>`: archivo donde se escribe la solución; una línea por depósito con los ids de los
  vendedores asignados a ese depósito.
- `[segundos]` (opcional): presupuesto de tiempo para la metaheurística. Si se omite, corre un
  número fijo de iteraciones.

Ejemplo (instancia real, una hora de presupuesto):

```bash
./gap_simulator instances/real/real_instance solucion_competencia.txt 3600
```

### Tests y experimentación

```bash
cd src
make tests                 # compila -> ./tests
./tests                    # corre todo: check + e1..e6 + convergencia + sinteticas
./tests check              # solo tests de correctitud
./tests e1                 # un experimento puntual (e1 .. e6)
./tests convergencia       # costo del incumbente vs tiempo (ILS y LNS)
./tests sinteticas         # peor/mejor caso sobre instancias sintéticas
```

Los resultados se escriben como CSV en `src/resultados/`.

### Figuras del informe

```bash
cd src
python3 plots.py           # lee resultados/ y la instancia real, genera figs/*.png
```

### Limpiar

```bash
cd src
make clean                 # borra binarios y archivos objeto
```
