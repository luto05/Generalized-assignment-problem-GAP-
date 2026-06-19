#include "Instancia.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace std;

Instancia::Instancia(string archivo) {
    ifstream entrada(archivo);
    if (!entrada.is_open()) {
        exit(1);
    }

    int m, n;
    entrada >> m >> n;

    vector<vector<double>> costos_archivo(m, vector<double>(n));
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            entrada >> costos_archivo[i][j];
        }
    }

    vector<int> demanda(n);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            int d;
            entrada >> d;
            if (i == 0) demanda[j] = d;
        }
    }

    vector<int> capacidad(m);
    for (int i = 0; i < m; i++) {
        entrada >> capacidad[i];
    }

    entrada.close();

    for (int i = 0; i < m; i++) {
        depositos.push_back(Deposito(i, capacidad[i]));
    }
    for (int j = 0; j < n; j++) {
        vector<double> dist(m);
        for (int i = 0; i < m; i++) {
            dist[i] = costos_archivo[i][j];
        }
        costo.push_back(dist);
        vendedores.push_back(Vendedor(j, demanda[j], dist));
    }
}
