#ifndef BUSQUEDA_LOCAL_H
#define BUSQUEDA_LOCAL_H

#include "Instancia.h"
#include "Solucion.h"

namespace busqueda_local {

bool relocate_first_improvement(Solucion& sol, const Instancia& inst);
bool relocate_best_improvement(Solucion& sol, const Instancia& inst);
bool swap_first_improvement(Solucion& sol, const Instancia& inst);
bool swap_best_improvement(Solucion& sol, const Instancia& inst);

// asigna a los vendedores sin asignar a un deposito factible (saca la
// penalizacion). devuelve true si asigno alguno.
bool asignar_no_asignados(Solucion& sol, const Instancia& inst);

// asigna a un no asignado sacando a otro de su deposito (eyeccion).
bool asignar_con_eyeccion(Solucion& sol, const Instancia& inst);

// relocate y swap generalizados: mueven/intercambian hasta 'cantidad'
// vendedores a la vez (cantidad=1 = el operador simple).
bool relocate_multiple(Solucion& sol, const Instancia& inst, int cantidad);
bool swap_multiple(Solucion& sol, const Instancia& inst, int cantidad);

// cyclic exchange de 3 depositos: rota j1->i2, j2->i3, j3->i1. 'vecinos' limita
// los depositos candidatos a los mas cercanos de cada vendedor.
bool ciclo3(Solucion& sol, const Instancia& inst, int vecinos);

}

#endif