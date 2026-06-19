#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include "Instancia.h"
#include "Solucion.h"
#include "HeuristicasConstructivas.h"
#include "Metaheuristicas.h"

using namespace std;

// escribe la solucion en el formato del enunciado:
// una linea por deposito con los indices de sus vendedores separados por espacio
void escribir_solucion(string archivo, Solucion sol) {
    ofstream salida(archivo);
    for (int i = 0; i < sol.vendedores_por_deposito.size(); i++) {
        for (int t = 0; t < sol.vendedores_por_deposito[i].size(); t++) {
            if (t > 0) salida << " ";
            salida << sol.vendedores_por_deposito[i][t];
        }
        salida << endl;
    }
    salida.close();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return 1;
    }

    Instancia inst(argv[1]);

    // opcional: argv[3] = presupuesto de tiempo en segundos (si no se pasa, corre 500 iteraciones)
    double segundos = (argc > 3) ? atof(argv[3]) : 0.0;
    Solucion sol = metaheuristicas::ils(inst, 500, 20, 1, segundos);

    cout << "Costo luego de ILS: " << sol.costo_total << endl;

    escribir_solucion(argv[2], sol);

    return 0;
}