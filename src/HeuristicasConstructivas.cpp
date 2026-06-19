#include "HeuristicasConstructivas.h"
#include <algorithm>

using namespace std;

namespace constructivas {

// Devuelve la mayor distancia que aparece entre todos los vendedores.
// Como las distancias de cada vendedor estan ordenadas de menor a mayor,
// la mas grande de cada uno queda en la ultima posicion (.back()).
// Sirve para fijar el costo de penalizacion de un vendedor sin asignar.
double distancia_maxima(const vector<Vendedor>& vendedores) {
    double maximo = 0;
    for (int j = 0; j < vendedores.size(); j++) {
        if (!vendedores[j].distancias.empty()) {
            maximo = max(maximo, vendedores[j].distancias.back().first);
        }
    }
    return maximo;
}

// Heuristica del vecino mas cercano.
// Recorre los vendedores en orden y asigna cada uno al deposito mas cercano
// que todavia tenga capacidad suficiente para cubrir su demanda.
Solucion vecino_mas_cercano(const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;

    Solucion sol;
    sol.vendedores_por_deposito.resize(depositos.size());
    sol.deposito_asignado.assign(vendedores.size(), -1);

    // Capacidad disponible de cada deposito, que se va descontando.
    vector<int> capacidad_restante;
    for (int i = 0; i < depositos.size(); i++) {
        capacidad_restante.push_back(depositos[i].capacidad);
    }

    // Costo que se suma cuando un vendedor no se puede asignar a ningun deposito.
    double penalizacion = 3 * distancia_maxima(vendedores);

    for (int j = 0; j < vendedores.size(); j++) {
        bool quedo_asignado = false;

        // Las distancias estan ordenadas, asi que el primer deposito factible
        // es directamente el mas cercano disponible.
        for (int t = 0; t < vendedores[j].distancias.size(); t++) {
            double distancia = vendedores[j].distancias[t].first;
            int deposito = vendedores[j].distancias[t].second;

            if (capacidad_restante[deposito] >= vendedores[j].demanda) {
                sol.asignar(deposito, j);
                sol.costo_total += distancia;
                capacidad_restante[deposito] -= vendedores[j].demanda;
                quedo_asignado = true;
                break;
            }
        }

        if (!quedo_asignado) {
            sol.costo_total += penalizacion;
        }
    }
    return sol;
}

// Heuristica de insercion basada en regret.
// En cada paso elige el vendedor "mas urgente": aquel cuya diferencia de costo
// entre su mejor y su segundo mejor deposito factible es mayor (mayor regret).
// La idea es atender primero a los vendedores que mas perderian si se les
// llenara su deposito preferido.
Solucion insercion(const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;

    int cantidad_vendedores = vendedores.size();
    Solucion sol;
    sol.vendedores_por_deposito.resize(depositos.size());
    sol.deposito_asignado.assign(cantidad_vendedores, -1);

    vector<int> capacidad_restante;
    for (int i = 0; i < depositos.size(); i++) {
        capacidad_restante.push_back(depositos[i].capacidad);
    }

    double penalizacion = 3 * distancia_maxima(vendedores);
    vector<bool> ya_asignado(cantidad_vendedores, false);

    // Se hace una asignacion por iteracion hasta colocar a todos los vendedores.
    for (int paso = 0; paso < cantidad_vendedores; paso++) {
        int vendedor_elegido = -1;
        int deposito_elegido = -1;
        double costo_elegido = 0;
        double mayor_regret = -1;
        bool hay_candidato = false;

        for (int j = 0; j < cantidad_vendedores; j++) {
            if (ya_asignado[j]) continue;

            // Busco los dos depositos factibles mas cercanos para este vendedor.
            int mejor_deposito = -1;
            double costo_mejor = 0, costo_segundo = 0;
            bool hay_segundo = false;

            for (int t = 0; t < vendedores[j].distancias.size(); t++) {
                int deposito = vendedores[j].distancias[t].second;
                if (capacidad_restante[deposito] >= vendedores[j].demanda) {
                    if (mejor_deposito == -1) {
                        mejor_deposito = deposito;
                        costo_mejor = vendedores[j].distancias[t].first;
                    } else {
                        costo_segundo = vendedores[j].distancias[t].first;
                        hay_segundo = true;
                        break;
                    }
                }
            }

            // Si no tiene ningun deposito factible, lo dejo para mas adelante.
            if (mejor_deposito == -1) continue;
            hay_candidato = true;

            // El regret mide cuanto se "arrepentiria" el vendedor de no usar su
            // mejor opcion. Si solo tiene una opcion factible, le doy prioridad
            // maxima con un valor muy grande.
            double regret;
            if (hay_segundo) {
                regret = costo_segundo - costo_mejor;
            } else {
                regret = 1e9;
            }

            if (regret > mayor_regret) {
                mayor_regret = regret;
                vendedor_elegido = j;
                deposito_elegido = mejor_deposito;
                costo_elegido = costo_mejor;
            }
        }

        // Si ningun vendedor restante tiene deposito factible, el resto queda
        // sin asignar y se les aplica la penalizacion.
        if (!hay_candidato) {
            for (int j = 0; j < cantidad_vendedores; j++) {
                if (!ya_asignado[j]) sol.costo_total += penalizacion;
            }
            break;
        }

        sol.asignar(deposito_elegido, vendedor_elegido);
        sol.costo_total += costo_elegido;
        capacidad_restante[deposito_elegido] -= vendedores[vendedor_elegido].demanda;
        ya_asignado[vendedor_elegido] = true;
    }
    return sol;
}

}
