#ifndef HEURISTICAS_CONSTRUCTIVAS_H
#define HEURISTICAS_CONSTRUCTIVAS_H

#include "Instancia.h"
#include "Solucion.h"

namespace constructivas {

// construye una solucion inicial asignando cada vendedor a su deposito factible
// mas cercano.
Solucion vecino_mas_cercano(const Instancia& inst);

// construye una solucion inicial por insercion: en cada paso elige la asignacion
// de menor costo entre las factibles.
Solucion insercion(const Instancia& inst);

}

#endif