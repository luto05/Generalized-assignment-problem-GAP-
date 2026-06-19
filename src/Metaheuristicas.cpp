#include "Metaheuristicas.h"
#include "HeuristicasConstructivas.h"
#include "BusquedaLocal.h"
#include <algorithm>
#include <random>
#include <chrono>

using namespace std;

namespace metaheuristicas {

// Mayor distancia entre un vendedor y un deposito (se usa para la penalizacion).
static double distancia_maxima(const Instancia& inst) {
    double maximo = 0;
    for (const Vendedor& v : inst.vendedores) {
        if (!v.distancias.empty()) maximo = max(maximo, v.distancias.back().first);
    }
    return maximo;
}

// Recalcula el costo total de una solucion desde cero: suma las distancias de los
// vendedores asignados y 3*distancia_maxima por cada vendedor sin asignar.
static double evaluar(const Solucion& sol, const Instancia& inst) {
    int cantidad_vendedores = inst.vendedores.size();
    double penalizacion = 3 * distancia_maxima(inst);
    double total = 0;
    for (int vendedor = 0; vendedor < cantidad_vendedores; vendedor++) {
        int deposito = sol.deposito_asignado[vendedor];
        if (deposito < 0) total += penalizacion;
        else total += inst.costo[vendedor][deposito];
    }
    return total;
}

// Capacidad que le queda libre a cada deposito en la solucion actual.
static vector<int> capacidad_libre(const Solucion& sol, const Instancia& inst) {
    vector<int> capacidad;
    for (int i = 0; i < (int)inst.depositos.size(); i++) capacidad.push_back(inst.depositos[i].capacidad);
    for (int vendedor = 0; vendedor < (int)inst.vendedores.size(); vendedor++) {
        int deposito = sol.deposito_asignado[vendedor];
        if (deposito >= 0) capacidad[deposito] -= inst.vendedores[vendedor].demanda;
    }
    return capacidad;
}

// VND (Variable Neighborhood Descent): aplica los operadores de busqueda local en
// orden, repitiendo cada uno mientras mejore, y vuelve a empezar la pasada cada
// vez que alguno mejoro. Termina en un optimo local respecto de todas las vecindades.
void vnd(Solucion& sol, const Instancia& inst) {
    const int Q_MAX = 10; // relocate/swap mueven hasta Q_MAX vendedores a la vez
    bool mejoro = true;
    while (mejoro) {
        mejoro = false;
        while (busqueda_local::asignar_no_asignados(sol, inst)) mejoro = true;
        while (busqueda_local::asignar_con_eyeccion(sol, inst)) mejoro = true;
        for (int q = 1; q <= Q_MAX; q++) {
            while (busqueda_local::relocate_multiple(sol, inst, q)) mejoro = true;
            while (busqueda_local::swap_multiple(sol, inst, q)) mejoro = true;
        }
        while (busqueda_local::ciclo3(sol, inst, 5)) mejoro = true;
    }
}

// Perturbacion mixta: mitad swaps al azar + mitad relocations al azar. Son golpes
// que pueden empeorar la solucion, justamente para poder escapar del optimo local.
// La mezcla salio levemente mejor que usar solo swaps.
static void perturbar(Solucion& sol, const Instancia& inst, int fuerza, mt19937& rng) {
    int cantidad_vendedores = inst.vendedores.size();
    int cantidad_depositos = inst.depositos.size();
    vector<int> capacidad = capacidad_libre(sol, inst);
    uniform_int_distribution<int> elegir_vendedor(0, cantidad_vendedores - 1);
    uniform_int_distribution<int> elegir_deposito(0, cantidad_depositos - 1);

    // Primera mitad: swaps al azar (intercambia dos vendedores entre sus depositos).
    int swaps = fuerza / 2;
    for (int t = 0; t < swaps; t++) {
        int vendedor1 = elegir_vendedor(rng), vendedor2 = elegir_vendedor(rng);
        if (vendedor1 == vendedor2) continue;
        int deposito1 = sol.deposito_asignado[vendedor1], deposito2 = sol.deposito_asignado[vendedor2];
        if (deposito1 < 0 || deposito2 < 0 || deposito1 == deposito2) continue;
        int demanda1 = inst.vendedores[vendedor1].demanda, demanda2 = inst.vendedores[vendedor2].demanda;
        if (demanda1 <= capacidad[deposito2] + demanda2 && demanda2 <= capacidad[deposito1] + demanda1) {
            sol.sacar(deposito1, vendedor1); sol.sacar(deposito2, vendedor2);
            sol.asignar(deposito2, vendedor1); sol.asignar(deposito1, vendedor2);
            capacidad[deposito1] += demanda1 - demanda2;
            capacidad[deposito2] += demanda2 - demanda1;
        }
    }
    // Segunda mitad: relocations al azar (mueve un vendedor a un deposito al azar con lugar).
    for (int t = swaps; t < fuerza; t++) {
        int vendedor = elegir_vendedor(rng);
        int deposito_actual = sol.deposito_asignado[vendedor];
        if (deposito_actual < 0) continue;
        int deposito_destino = elegir_deposito(rng);
        if (deposito_destino == deposito_actual) continue;
        int demanda = inst.vendedores[vendedor].demanda;
        if (capacidad[deposito_destino] >= demanda) {
            sol.sacar(deposito_actual, vendedor); sol.asignar(deposito_destino, vendedor);
            capacidad[deposito_actual] += demanda; capacidad[deposito_destino] -= demanda;
        }
    }
}

// ILS (Iterated Local Search): parte de una solucion constructiva + vnd y repite
// el ciclo (perturbar + vnd), quedandose siempre con la mejor solucion encontrada.
Solucion ils(const Instancia& inst, int iteraciones, int fuerza, unsigned semilla, double segundos) {
    mt19937 rng(semilla);

    // Arranco de la mejor de las dos constructivas y la optimizo con el vnd.
    Solucion mejor = constructivas::vecino_mas_cercano(inst);
    Solucion alternativa = constructivas::insercion(inst);
    if (alternativa.costo_total < mejor.costo_total) mejor = alternativa;
    vnd(mejor, inst);
    mejor.costo_total = evaluar(mejor, inst);

    // Si segundos > 0 corta por tiempo; si no, por cantidad de iteraciones.
    auto inicio = chrono::high_resolution_clock::now();
    for (int it = 0; segundos > 0 || it < iteraciones; it++) {
        if (segundos > 0) {
            double pasado = chrono::duration_cast<chrono::milliseconds>(
                chrono::high_resolution_clock::now() - inicio).count() / 1000.0;
            if (pasado >= segundos) break;
        }
        Solucion candidata = mejor;
        perturbar(candidata, inst, fuerza, rng);
        candidata.costo_total = evaluar(candidata, inst);
        vnd(candidata, inst);
        if (candidata.costo_total < mejor.costo_total) mejor = candidata;
    }
    return mejor;
}

// LNS (Large Neighborhood Search, estilo ruin-and-recreate): en cada iteracion
// desasigna 'destruir' vendedores al azar y deja que el vnd los reinserte (con
// asignar_no_asignados) y optimice. Es una perturbacion mas "grande" que la de
// ILS. Acepta el resultado solo si mejora.
Solucion lns(const Instancia& inst, int iteraciones, int destruir, unsigned semilla, double segundos) {
    mt19937 rng(semilla);

    Solucion mejor = constructivas::vecino_mas_cercano(inst);
    Solucion alternativa = constructivas::insercion(inst);
    if (alternativa.costo_total < mejor.costo_total) mejor = alternativa;
    vnd(mejor, inst);
    mejor.costo_total = evaluar(mejor, inst);

    int cantidad_vendedores = inst.vendedores.size();
    auto inicio = chrono::high_resolution_clock::now();
    for (int it = 0; segundos > 0 || it < iteraciones; it++) {
        if (segundos > 0) {
            double pasado = chrono::duration_cast<chrono::milliseconds>(
                chrono::high_resolution_clock::now() - inicio).count() / 1000.0;
            if (pasado >= segundos) break;
        }
        Solucion candidata = mejor;
        // Destruir: desasignar 'destruir' vendedores elegidos al azar.
        vector<int> asignados;
        for (int vendedor = 0; vendedor < cantidad_vendedores; vendedor++) {
            if (candidata.deposito_asignado[vendedor] >= 0) asignados.push_back(vendedor);
        }
        shuffle(asignados.begin(), asignados.end(), rng);
        for (int t = 0; t < destruir && t < (int)asignados.size(); t++) {
            int vendedor = asignados[t];
            candidata.sacar(candidata.deposito_asignado[vendedor], vendedor);
        }
        // Recrear + optimizar: el vnd reinserta los desasignados y mejora.
        vnd(candidata, inst);
        candidata.costo_total = evaluar(candidata, inst);
        if (candidata.costo_total < mejor.costo_total) mejor = candidata;
    }
    return mejor;
}

}
