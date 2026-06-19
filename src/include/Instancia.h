#ifndef INSTANCIA_H
#define INSTANCIA_H

#include <string>
#include <vector>
#include "Deposito.h"
#include "Vendedor.h"

// datos de una instancia del problema: depositos, vendedores y la matriz de
// costos de asignar cada vendedor a cada deposito. se carga desde un archivo.
class Instancia {
public:
    std::vector<Deposito> depositos;
    std::vector<Vendedor> vendedores;
    std::vector<std::vector<double>> costo;

    Instancia(std::string archivo);
};

#endif