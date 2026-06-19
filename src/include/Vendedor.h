#ifndef VENDEDOR_H
#define VENDEDOR_H

#include <vector>
#include <algorithm>

// vendedor a asignar: tiene un id, una demanda y la lista de
// distancias a cada deposito, ordenada de menor a mayor (con el id del deposito).
class Vendedor {
public:
    int id;
    int demanda;
    std::vector<std::pair<double, int>> distancias;

    Vendedor(int id, int demanda, std::vector<double> dist) {
        this->id = id;
        this->demanda = demanda;
        for (int i = 0; i < dist.size(); i++) {
            distancias.push_back(std::make_pair(dist[i], i)); // Agregamos distancia al deposito junto al id
        }
        std::sort(distancias.begin(), distancias.end()); // Ordenamos por distancia
    }
};

#endif