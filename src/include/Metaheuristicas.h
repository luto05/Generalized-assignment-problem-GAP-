#ifndef METAHEURISTICAS_H
#define METAHEURISTICAS_H

#include "Instancia.h"
#include "Solucion.h"

namespace metaheuristicas {

// busqueda local: relocate + swap (first improvement) hasta optimo local
void vnd(Solucion& sol, const Instancia& inst);

// ILS: arranca de una constructiva + vnd, y repite (perturbar + vnd),
// quedandose siempre con la mejor solucion. 'fuerza' = cantidad de swaps al azar.
// Si 'segundos' > 0, corta por tiempo en vez de por cantidad de iteraciones.
Solucion ils(const Instancia& inst, int iteraciones = 100, int fuerza = 10, unsigned semilla = 1, double segundos = 0);

// LNS (ruin-and-recreate): desasigna 'destruir' vendedores al azar y los
// reinserta/optimiza con el vnd. Si 'segundos' > 0, corta por tiempo.
Solucion lns(const Instancia& inst, int iteraciones = 100, int destruir = 50, unsigned semilla = 1, double segundos = 0);

}

#endif
