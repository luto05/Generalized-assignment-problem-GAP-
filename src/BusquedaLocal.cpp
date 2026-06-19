#include "BusquedaLocal.h"
#include <algorithm>

using namespace std;

namespace busqueda_local {

// Calcula la capacidad que le queda libre a cada deposito dada una solucion:
// parte de la capacidad total y le resta la demanda de los vendedores asignados.
vector<int> capacidad_libre(const Solucion& sol, const vector<Deposito>& depositos, const vector<Vendedor>& vendedores) {
    vector<int> capacidad;
    for (int i = 0; i < depositos.size(); i++) {
        capacidad.push_back(depositos[i].capacidad);
    }
    for (int vendedor = 0; vendedor < vendedores.size(); vendedor++) {
        int deposito = sol.deposito_asignado[vendedor];
        if (deposito >= 0) capacidad[deposito] -= vendedores[vendedor].demanda;
    }
    return capacidad;
}

// Mayor distancia entre un vendedor y un deposito (se usa para la penalizacion).
static double distancia_maxima(const Instancia& inst) {
    double maximo = 0;
    for (const Vendedor& v : inst.vendedores) {
        if (!v.distancias.empty()) maximo = max(maximo, v.distancias.back().first);
    }
    return maximo;
}

// Intenta asignar a los vendedores que quedaron sin asignar (deposito_asignado
// == -1) a su deposito factible mas cercano. Al asignarlos se saca la
// penalizacion de 3*distancia_maxima, asi que entrar en cualquier deposito
// siempre mejora. Devuelve true si asigno alguno. Es lo que reconsidera a los no
// asignados cuando relocate/swap liberan capacidad.
bool asignar_no_asignados(Solucion& sol, const Instancia& inst) {
    const vector<Vendedor>& vendedores = inst.vendedores;
    int cantidad_vendedores = vendedores.size();
    vector<int> capacidad = capacidad_libre(sol, inst.depositos, vendedores);
    double penalizacion = 3 * distancia_maxima(inst);

    bool hubo_cambio = false;
    for (int vendedor = 0; vendedor < cantidad_vendedores; vendedor++) {
        if (sol.deposito_asignado[vendedor] >= 0) continue; // ya asignado
        int demanda = vendedores[vendedor].demanda;
        // Las distancias estan ordenadas: el primer deposito factible es el mas cercano.
        for (int t = 0; t < (int)vendedores[vendedor].distancias.size(); t++) {
            int deposito = vendedores[vendedor].distancias[t].second;
            if (capacidad[deposito] >= demanda) {
                sol.asignar(deposito, vendedor);
                sol.costo_total += vendedores[vendedor].distancias[t].first - penalizacion;
                capacidad[deposito] -= demanda;
                hubo_cambio = true;
                break;
            }
        }
    }
    return hubo_cambio;
}

// Eyeccion: para un vendedor sin asignar lo mete en un deposito k sacando a un
// vendedor ya asignado de k (y reubicando a ese en otro deposito con lugar).
// Como meter al no asignado saca su penalizacion de 3*distancia_maxima, suele
// mejorar aunque el desplazado quede algo peor. Sirve para colocar no asignados
// en instancias ajustadas. Aplica la primera mejora que encuentra.
bool asignar_con_eyeccion(Solucion& sol, const Instancia& inst) {
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_depositos = inst.depositos.size();
    vector<int> capacidad = capacidad_libre(sol, inst.depositos, vendedores);
    double penalizacion = 3 * distancia_maxima(inst);

    for (int vendedor = 0; vendedor < (int)vendedores.size(); vendedor++) {
        if (sol.deposito_asignado[vendedor] >= 0) continue; // solo no asignados
        int demanda = vendedores[vendedor].demanda;
        for (int t = 0; t < (int)vendedores[vendedor].distancias.size(); t++) {
            int k = vendedores[vendedor].distancias[t].second;
            if (capacidad[k] >= demanda) continue;   // si entra directo lo hace asignar_no_asignados
            int falta_capacidad = demanda - capacidad[k]; // capacidad a liberar en k
            for (int desplazado : sol.vendedores_por_deposito[k]) {
                int demanda_desplazado = vendedores[desplazado].demanda;
                if (demanda_desplazado < falta_capacidad) continue; // sacarlo no alcanza
                for (int otro_deposito = 0; otro_deposito < cantidad_depositos; otro_deposito++) {
                    if (otro_deposito == k || capacidad[otro_deposito] < demanda_desplazado) continue;
                    double delta = (costo[vendedor][k] - penalizacion)
                                 + (costo[desplazado][otro_deposito] - costo[desplazado][k]);
                    if (delta < 0) {
                        sol.sacar(k, desplazado); sol.asignar(otro_deposito, desplazado);
                        sol.asignar(k, vendedor);
                        sol.costo_total += delta;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Relocate (primera mejora): mueve un vendedor a otro deposito mas barato que
// tenga lugar. Se queda con el primer movimiento que mejore.
bool relocate_first_improvement(Solucion& sol, const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_vendedores = vendedores.size();
    int cantidad_depositos = depositos.size();
    vector<int> capacidad = capacidad_libre(sol, depositos, vendedores);

    for (int vendedor = 0; vendedor < cantidad_vendedores; vendedor++) {
        int deposito_actual = sol.deposito_asignado[vendedor];
        if (deposito_actual < 0) continue;
        for (int deposito_destino = 0; deposito_destino < cantidad_depositos; deposito_destino++) {
            if (deposito_destino == deposito_actual) continue;
            if (costo[vendedor][deposito_destino] < costo[vendedor][deposito_actual]
                && vendedores[vendedor].demanda <= capacidad[deposito_destino]) {
                sol.sacar(deposito_actual, vendedor);
                sol.asignar(deposito_destino, vendedor);
                sol.costo_total += costo[vendedor][deposito_destino] - costo[vendedor][deposito_actual];
                return true;
            }
        }
    }
    return false;
}

// Relocate (mejor mejora): recorre todos los movimientos posibles y aplica el
// que mas reduce el costo.
bool relocate_best_improvement(Solucion& sol, const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_vendedores = vendedores.size();
    int cantidad_depositos = depositos.size();
    vector<int> capacidad = capacidad_libre(sol, depositos, vendedores);

    bool hay_mejora = false;
    double mejor_delta = 0;
    int mejor_vendedor = -1, mejor_origen = -1, mejor_destino = -1;

    for (int vendedor = 0; vendedor < cantidad_vendedores; vendedor++) {
        int deposito_actual = sol.deposito_asignado[vendedor];
        if (deposito_actual < 0) continue;
        for (int deposito_destino = 0; deposito_destino < cantidad_depositos; deposito_destino++) {
            if (deposito_destino == deposito_actual) continue;
            if (costo[vendedor][deposito_destino] < costo[vendedor][deposito_actual]
                && vendedores[vendedor].demanda <= capacidad[deposito_destino]) {
                double delta = costo[vendedor][deposito_destino] - costo[vendedor][deposito_actual];
                if (!hay_mejora || delta < mejor_delta) {
                    hay_mejora = true;
                    mejor_delta = delta;
                    mejor_vendedor = vendedor;
                    mejor_origen = deposito_actual;
                    mejor_destino = deposito_destino;
                }
            }
        }
    }

    if (hay_mejora) {
        sol.sacar(mejor_origen, mejor_vendedor);
        sol.asignar(mejor_destino, mejor_vendedor);
        sol.costo_total += mejor_delta;
    }
    return hay_mejora;
}

// Swap (primera mejora): intercambia los depositos de dos vendedores si el
// cambio reduce el costo y ambos siguen entrando. Aplica el primero que mejore.
bool swap_first_improvement(Solucion& sol, const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_vendedores = vendedores.size();
    vector<int> capacidad = capacidad_libre(sol, depositos, vendedores);

    for (int vendedor1 = 0; vendedor1 < cantidad_vendedores; vendedor1++) {
        int deposito1 = sol.deposito_asignado[vendedor1];
        if (deposito1 < 0) continue;
        for (int vendedor2 = vendedor1 + 1; vendedor2 < cantidad_vendedores; vendedor2++) {
            int deposito2 = sol.deposito_asignado[vendedor2];
            if (deposito2 < 0 || deposito1 == deposito2) continue;

            int demanda1 = vendedores[vendedor1].demanda;
            int demanda2 = vendedores[vendedor2].demanda;

            double delta = (costo[vendedor1][deposito2] + costo[vendedor2][deposito1])
                         - (costo[vendedor1][deposito1] + costo[vendedor2][deposito2]);
            // Al intercambiar, cada deposito recupera la demanda del que sale.
            bool entra1 = demanda1 <= capacidad[deposito2] + demanda2;
            bool entra2 = demanda2 <= capacidad[deposito1] + demanda1;

            if (delta < 0 && entra1 && entra2) {
                sol.sacar(deposito1, vendedor1);
                sol.sacar(deposito2, vendedor2);
                sol.asignar(deposito2, vendedor1);
                sol.asignar(deposito1, vendedor2);
                sol.costo_total += delta;
                return true;
            }
        }
    }
    return false;
}

// Swap (mejor mejora): evalua todos los intercambios factibles y aplica el de
// mayor reduccion de costo.
bool swap_best_improvement(Solucion& sol, const Instancia& inst) {
    const vector<Deposito>& depositos = inst.depositos;
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_vendedores = vendedores.size();
    vector<int> capacidad = capacidad_libre(sol, depositos, vendedores);

    bool hay_mejora = false;
    double mejor_delta = 0;
    int mejor_v1 = -1, mejor_d1 = -1, mejor_v2 = -1, mejor_d2 = -1;

    for (int vendedor1 = 0; vendedor1 < cantidad_vendedores; vendedor1++) {
        int deposito1 = sol.deposito_asignado[vendedor1];
        if (deposito1 < 0) continue;
        for (int vendedor2 = vendedor1 + 1; vendedor2 < cantidad_vendedores; vendedor2++) {
            int deposito2 = sol.deposito_asignado[vendedor2];
            if (deposito2 < 0 || deposito1 == deposito2) continue;

            int demanda1 = vendedores[vendedor1].demanda;
            int demanda2 = vendedores[vendedor2].demanda;

            double delta = (costo[vendedor1][deposito2] + costo[vendedor2][deposito1])
                         - (costo[vendedor1][deposito1] + costo[vendedor2][deposito2]);
            bool entra1 = demanda1 <= capacidad[deposito2] + demanda2;
            bool entra2 = demanda2 <= capacidad[deposito1] + demanda1;

            if (delta < 0 && entra1 && entra2) {
                if (!hay_mejora || delta < mejor_delta) {
                    hay_mejora = true;
                    mejor_delta = delta;
                    mejor_v1 = vendedor1;
                    mejor_d1 = deposito1;
                    mejor_v2 = vendedor2;
                    mejor_d2 = deposito2;
                }
            }
        }
    }

    if (hay_mejora) {
        sol.sacar(mejor_d1, mejor_v1);
        sol.sacar(mejor_d2, mejor_v2);
        sol.asignar(mejor_d2, mejor_v1);
        sol.asignar(mejor_d1, mejor_v2);
        sol.costo_total += mejor_delta;
    }
    return hay_mejora;
}

// Relocate generalizado: mueve hasta 'cantidad' vendedores convenientes de un
// deposito origen a otro destino (los de mayor ganancia que entren). cantidad=1
// equivale a un relocate simple. Aplica en el primer par (origen, destino) que mejore.
bool relocate_multiple(Solucion& sol, const Instancia& inst, int cantidad) {
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_depositos = inst.depositos.size();
    vector<int> capacidad = capacidad_libre(sol, inst.depositos, vendedores);

    for (int origen = 0; origen < cantidad_depositos; origen++) {
        if (sol.vendedores_por_deposito[origen].empty()) continue;
        for (int destino = 0; destino < cantidad_depositos; destino++) {
            if (destino == origen) continue;
            // Candidatos: vendedores de 'origen' que mejoran al pasar a 'destino',
            // guardados junto con su ganancia (negativa = conviene).
            vector<pair<double, int>> candidatos;
            for (int vendedor : sol.vendedores_por_deposito[origen]) {
                double ganancia = costo[vendedor][destino] - costo[vendedor][origen];
                if (ganancia < 0) candidatos.push_back(make_pair(ganancia, vendedor));
            }
            if (candidatos.empty()) continue;
            sort(candidatos.begin(), candidatos.end()); // mas convenientes primero

            int espacio_libre = capacidad[destino];
            double delta = 0;
            vector<int> a_mover;
            for (int idx = 0; idx < (int)candidatos.size() && (int)a_mover.size() < cantidad; idx++) {
                int vendedor = candidatos[idx].second;
                int demanda = vendedores[vendedor].demanda;
                if (demanda <= espacio_libre) {
                    a_mover.push_back(vendedor);
                    espacio_libre -= demanda;
                    delta += candidatos[idx].first;
                }
            }
            if (!a_mover.empty()) {
                for (int vendedor : a_mover) { sol.sacar(origen, vendedor); sol.asignar(destino, vendedor); }
                sol.costo_total += delta;
                return true;
            }
        }
    }
    return false;
}

// Swap generalizado: aplica hasta 'cantidad' intercambios sucesivos (el mejor
// cada vez) entre un par de depositos. cantidad=1 equivale a un swap simple.
// Aplica en el primer par de depositos que mejore.
bool swap_multiple(Solucion& sol, const Instancia& inst, int cantidad) {
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_depositos = inst.depositos.size();
    vector<int> capacidad = capacidad_libre(sol, inst.depositos, vendedores);

    for (int deposito1 = 0; deposito1 < cantidad_depositos; deposito1++) {
        for (int deposito2 = deposito1 + 1; deposito2 < cantidad_depositos; deposito2++) {
            double delta_total = 0;
            int intercambios_hechos = 0;
            while (intercambios_hechos < cantidad) {
                // Busco el mejor intercambio factible entre estos dos depositos.
                double mejor_delta = 0;
                int mejor_v1 = -1, mejor_v2 = -1;
                for (int vendedor1 : sol.vendedores_por_deposito[deposito1]) {
                    for (int vendedor2 : sol.vendedores_por_deposito[deposito2]) {
                        int demanda1 = vendedores[vendedor1].demanda;
                        int demanda2 = vendedores[vendedor2].demanda;
                        double delta = (costo[vendedor1][deposito2] + costo[vendedor2][deposito1])
                                     - (costo[vendedor1][deposito1] + costo[vendedor2][deposito2]);
                        if (delta < mejor_delta
                            && demanda1 <= capacidad[deposito2] + demanda2
                            && demanda2 <= capacidad[deposito1] + demanda1) {
                            mejor_delta = delta; mejor_v1 = vendedor1; mejor_v2 = vendedor2;
                        }
                    }
                }
                if (mejor_v1 < 0) break; // no hay mas intercambios que mejoren
                int demanda1 = vendedores[mejor_v1].demanda;
                int demanda2 = vendedores[mejor_v2].demanda;
                sol.sacar(deposito1, mejor_v1); sol.sacar(deposito2, mejor_v2);
                sol.asignar(deposito2, mejor_v1); sol.asignar(deposito1, mejor_v2);
                capacidad[deposito1] += demanda1 - demanda2;
                capacidad[deposito2] += demanda2 - demanda1;
                delta_total += mejor_delta;
                intercambios_hechos++;
            }
            if (intercambios_hechos > 0) {
                sol.costo_total += delta_total;
                return true;
            }
        }
    }
    return false;
}

// Cyclic exchange de 3 depositos: rota tres vendedores entre tres depositos
// (vendedor1 -> deposito2, vendedor2 -> deposito3, vendedor3 -> deposito1). Son
// movidas que relocate/swap (solo 2 depositos) no pueden alcanzar. Para que sea
// tratable, los depositos destino se limitan a los 'vecinos' mas cercanos de cada
// vendedor (las distancias ya estan ordenadas). Usa un epsilon para no aceptar
// mejoras que en realidad son ruido de punto flotante (lo que evitaria loops
// infinitos). Aplica la primera mejora.
bool ciclo3(Solucion& sol, const Instancia& inst, int vecinos) {
    const double EPS = 1e-7;
    const vector<Vendedor>& vendedores = inst.vendedores;
    const vector<vector<double>>& costo = inst.costo;
    int cantidad_vendedores = vendedores.size();
    vector<int> capacidad = capacidad_libre(sol, inst.depositos, vendedores);

    for (int vendedor1 = 0; vendedor1 < cantidad_vendedores; vendedor1++) {
        int deposito1 = sol.deposito_asignado[vendedor1];
        if (deposito1 < 0) continue;
        int demanda1 = vendedores[vendedor1].demanda;

        // Recorro como deposito2 los vecinos mas cercanos de vendedor1.
        int vecinos_vistos_2 = 0;
        for (int a = 0; a < (int)vendedores[vendedor1].distancias.size() && vecinos_vistos_2 < vecinos; a++) {
            int deposito2 = vendedores[vendedor1].distancias[a].second;
            if (deposito2 == deposito1) continue;
            vecinos_vistos_2++;
            for (int idx2 = 0; idx2 < (int)sol.vendedores_por_deposito[deposito2].size(); idx2++) {
                int vendedor2 = sol.vendedores_por_deposito[deposito2][idx2];
                int demanda2 = vendedores[vendedor2].demanda;

                // Y como deposito3 los vecinos mas cercanos de vendedor2.
                int vecinos_vistos_3 = 0;
                for (int b = 0; b < (int)vendedores[vendedor2].distancias.size() && vecinos_vistos_3 < vecinos; b++) {
                    int deposito3 = vendedores[vendedor2].distancias[b].second;
                    if (deposito3 == deposito1 || deposito3 == deposito2) continue;
                    vecinos_vistos_3++;
                    for (int idx3 = 0; idx3 < (int)sol.vendedores_por_deposito[deposito3].size(); idx3++) {
                        int vendedor3 = sol.vendedores_por_deposito[deposito3][idx3];
                        int demanda3 = vendedores[vendedor3].demanda;
                        // Factibilidad de la rotacion v1->d2, v2->d3, v3->d1.
                        if (demanda1 <= capacidad[deposito2] + demanda2
                            && demanda2 <= capacidad[deposito3] + demanda3
                            && demanda3 <= capacidad[deposito1] + demanda1) {
                            double delta = (costo[vendedor1][deposito2] + costo[vendedor2][deposito3] + costo[vendedor3][deposito1])
                                         - (costo[vendedor1][deposito1] + costo[vendedor2][deposito2] + costo[vendedor3][deposito3]);
                            if (delta < -EPS) {
                                sol.sacar(deposito1, vendedor1); sol.sacar(deposito2, vendedor2); sol.sacar(deposito3, vendedor3);
                                sol.asignar(deposito2, vendedor1); sol.asignar(deposito3, vendedor2); sol.asignar(deposito1, vendedor3);
                                sol.costo_total += delta;
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

}
