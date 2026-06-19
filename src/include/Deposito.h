#ifndef DEPOSITO_H
#define DEPOSITO_H


class Deposito {
public:
    int id; // id del deposito
    int capacidad; // capacidad maxima del deposito

    Deposito(int id, int capacidad) {
        this->id = id;
        this->capacidad = capacidad;
    }
};

#endif