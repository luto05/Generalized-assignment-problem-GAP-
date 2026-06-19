#ifndef SOLUCION_H
#define SOLUCION_H

#include <vector>

// solucion del problema: guarda que vendedores tiene cada deposito, a que
// deposito esta asignado cada vendedor y el costo total.
class Solucion {
    public:
        std::vector<std::vector<int>> vendedores_por_deposito;
        std::vector<int> deposito_asignado;
        double costo_total = 0;

        // quita el vendedor del deposito y lo deja sin asignar.
        void sacar(int deposito, int vendedor) {
            deposito_asignado[vendedor] = -1;
            vendedores_por_deposito[deposito].erase(find(vendedores_por_deposito[deposito].begin(), vendedores_por_deposito[deposito].end(), vendedor));

        }

        // asigna el vendedor al deposito.
        void asignar(int deposito, int vendedor) {

            deposito_asignado[vendedor] = deposito;
            vendedores_por_deposito[deposito].push_back(vendedor);
        }
};

#endif