// =============================================================================
//  tests.cpp  -  Suite de tests y experimentacion para el GAP (ThunderPack)
//
//  Un unico driver que:
//    1) verifica correctitud (factibilidad + costo incremental == recomputado);
//    2) corre todos los metodos sobre las 28 instancias provistas y emite los
//       CSV que llenan las tablas E1-E6 del informe (mejor/peor/promedio =
//       peor caso / mejor caso empirico);
//    3) genera instancias sinteticas que disparan el peor/mejor caso de cada
//       tipo de metodo (constructivas, busqueda local, metaheuristica).
//
//  COMPILAR (desde src/):  make tests
//  EJECUTAR:
//    ./tests            corre todo (check + e1..e6 + sinteticas)
//    ./tests check      solo tests de correctitud
//    ./tests e1 .. e6   un experimento puntual
//    ./tests sinteticas demostracion peor/mejor caso
//
//  Los CSV se escriben en src/resultados/.
//  NOTA: ejecutar parado en src/ (las rutas de las instancias son relativas).
// =============================================================================

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef _WIN32
#include <direct.h>
static void crear_dir(const std::string& d) { _mkdir(d.c_str()); }
#else
#include <sys/stat.h>
static void crear_dir(const std::string& d) { mkdir(d.c_str(), 0755); }
#endif

#include "Instancia.h"
#include "Solucion.h"
#include "HeuristicasConstructivas.h"
#include "BusquedaLocal.h"
#include "Metaheuristicas.h"

using namespace std;

// -----------------------------------------------------------------------------
//  Helpers de evaluacion / verificacion (independientes de los algoritmos)
// -----------------------------------------------------------------------------

// maxima distancia vendedor-deposito (igual que en los algoritmos)
static double cmax(const Instancia& inst) {
    double m = 0;
    for (const auto& fila : inst.costo)
        for (double c : fila) m = max(m, c);
    return m;
}

// costo total recomputado desde cero (eq. de penalizacion del informe):
// suma de distancias asignadas + 3*cmax por cada vendedor sin asignar.
static double evaluar(const Solucion& sol, const Instancia& inst) {
    int n = inst.vendedores.size();
    double pen = 3 * cmax(inst);
    double total = 0;
    for (int j = 0; j < n; j++) {
        int dep = sol.deposito_asignado[j];
        if (dep < 0) total += pen;
        else total += inst.costo[j][dep];
    }
    return total;
}

static int no_asignados(const Solucion& sol) {
    int c = 0;
    for (int d : sol.deposito_asignado) if (d < 0) c++;
    return c;
}

// distancia media de los vendedores efectivamente asignados (P2)
static double distancia_media(const Solucion& sol, const Instancia& inst) {
    int n = inst.vendedores.size();
    double suma = 0; int asign = 0;
    for (int j = 0; j < n; j++) {
        int dep = sol.deposito_asignado[j];
        if (dep >= 0) { suma += inst.costo[j][dep]; asign++; }
    }
    return asign > 0 ? suma / asign : 0;
}

// verifica que no se exceda ninguna capacidad y que las dos estructuras de la
// solucion (deposito_asignado y vendedores_por_deposito) sean consistentes.
static bool factible(const Solucion& sol, const Instancia& inst) {
    int m = inst.depositos.size();
    int n = inst.vendedores.size();
    vector<int> uso(m, 0);
    for (int j = 0; j < n; j++) {
        int dep = sol.deposito_asignado[j];
        if (dep >= 0) uso[dep] += inst.vendedores[j].demanda;
    }
    for (int i = 0; i < m; i++)
        if (uso[i] > inst.depositos[i].capacidad) return false;
    // consistencia de las dos vistas de la asignacion
    vector<int> cuenta(n, 0);
    for (int i = 0; i < m; i++)
        for (int j : sol.vendedores_por_deposito[i]) {
            if (sol.deposito_asignado[j] != i) return false;
            cuenta[j]++;
        }
    for (int j = 0; j < n; j++) {
        int dep = sol.deposito_asignado[j];
        if (dep >= 0 && cuenta[j] != 1) return false;
        if (dep < 0 && cuenta[j] != 0) return false;
    }
    return true;
}

template <class F>
static double cronometrar(F&& f) {
    auto t0 = chrono::high_resolution_clock::now();
    f();
    auto t1 = chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1e6;
}

// -----------------------------------------------------------------------------
//  Registro de instancias
// -----------------------------------------------------------------------------

struct InstInfo {
    string path;
    string nombre;
    char grupo;     // 'a', 'b', 'e', 'r' (real)
    int n;          // cantidad de vendedores (para el tier de tiempo)
};

// presupuesto de tiempo (segundos) para las metaheuristicas, por tamano
static double tier_segundos(int n) {
    if (n <= 200) return 2.0;     // chicas
    if (n <= 900) return 10.0;    // medianas
    return 60.0;                  // grandes (real, e*1600)
}

static vector<InstInfo> instancias() {
    auto A = [](string p, string nm, char g, int n) { return InstInfo{p, nm, g, n}; };
    string a = "instances/gap/gap_a/";
    string b = "instances/gap/gap_b/";
    string e = "instances/gap/gap_e/";
    return {
        A(a + "a05100", "a05100", 'a', 100), A(a + "a05200", "a05200", 'a', 200),
        A(a + "a10100", "a10100", 'a', 100), A(a + "a10200", "a10200", 'a', 200),
        A(a + "a20100", "a20100", 'a', 100), A(a + "a20200", "a20200", 'a', 200),

        A(b + "b05100", "b05100", 'b', 100), A(b + "b05200", "b05200", 'b', 200),
        A(b + "b10100", "b10100", 'b', 100), A(b + "b10200", "b10200", 'b', 200),
        A(b + "b20100", "b20100", 'b', 100), A(b + "b20200", "b20200", 'b', 200),

        A(e + "e05100", "e05100", 'e', 100), A(e + "e05200", "e05200", 'e', 200),
        A(e + "e10100", "e10100", 'e', 100), A(e + "e10200", "e10200", 'e', 200),
        A(e + "e10400", "e10400", 'e', 400), A(e + "e20100", "e20100", 'e', 100),
        A(e + "e20200", "e20200", 'e', 200), A(e + "e20400", "e20400", 'e', 400),
        A(e + "e40400", "e40400", 'e', 400), A(e + "e15900", "e15900", 'e', 900),
        A(e + "e30900", "e30900", 'e', 900), A(e + "e60900", "e60900", 'e', 900),
        A(e + "e201600", "e201600", 'e', 1600), A(e + "e401600", "e401600", 'e', 1600),
        A(e + "e801600", "e801600", 'e', 1600),

        A("instances/real/real_instance", "real", 'r', 1100),
    };
}

static const char* OUTDIR = "resultados";

static void asegurar_outdir() {
    crear_dir(OUTDIR);
}

static string out(const string& f) { return string(OUTDIR) + "/" + f; }

// configuracion de las metaheuristicas elegida por defecto para E5/E6 (ver tuning E4)
static const int DEFAULT_FUERZA = 20;          // perturbacion de ILS
static const unsigned SEMILLAS = 5;            // semillas 1..SEMILLAS

// tamano de destruccion de LNS: del tuning E4, las instancias ajustadas chicas/
// medianas rinden mejor con destruir chico (20) y las grandes/real con destruir
// grande (100).
static int lns_destruir(int n) { return n > 900 ? 100 : 20; }

// estadisticas resumidas de un conjunto de costos (mejor/peor/media/desvio)
struct Stats { double mejor, peor, media, desvio; };
static Stats resumir(const vector<double>& v) {
    Stats s{0, 0, 0, 0};
    if (v.empty()) return s;
    s.mejor = *min_element(v.begin(), v.end());
    s.peor  = *max_element(v.begin(), v.end());
    s.media = accumulate(v.begin(), v.end(), 0.0) / v.size();
    double acc = 0;
    for (double x : v) acc += (x - s.media) * (x - s.media);
    s.desvio = sqrt(acc / v.size());
    return s;
}

// -----------------------------------------------------------------------------
//  Tests de correctitud
// -----------------------------------------------------------------------------

static int g_pass = 0, g_fail = 0;
static void chequear(bool ok, const string& desc) {
    if (ok) { g_pass++; }
    else { g_fail++; cout << "  [FAIL] " << desc << "\n"; }
}

static void test_check() {
    cout << "== Tests de correctitud ==\n";
    const double EPS = 1e-6;
    // instancias chicas representativas (una de cada grupo)
    vector<string> casos = {
        "instances/gap/gap_a/a05100",
        "instances/gap/gap_b/b05100",
        "instances/gap/gap_e/e05100",
    };
    for (const string& path : casos) {
        Instancia inst(path);

        // --- constructivas: factibles y costo_total coincide con evaluar ---
        Solucion vmc = constructivas::vecino_mas_cercano(inst);
        chequear(factible(vmc, inst), path + ": vecino_mas_cercano factible");
        chequear(fabs(vmc.costo_total - evaluar(vmc, inst)) < EPS,
                 path + ": vecino_mas_cercano costo_total == evaluar");

        Solucion ins = constructivas::insercion(inst);
        chequear(factible(ins, inst), path + ": insercion factible");
        chequear(fabs(ins.costo_total - evaluar(ins, inst)) < EPS,
                 path + ": insercion costo_total == evaluar");

        // --- busqueda local: el costo incremental (delta) sigue coincidiendo
        //     con el recomputado, y la solucion sigue factible ---
        vector<pair<string, function<bool(Solucion&, const Instancia&)>>> ops = {
            {"relocate_first", busqueda_local::relocate_first_improvement},
            {"relocate_best",  busqueda_local::relocate_best_improvement},
            {"swap_first",     busqueda_local::swap_first_improvement},
            {"swap_best",      busqueda_local::swap_best_improvement},
        };
        for (auto& op : ops) {
            Solucion s = vmc;
            while (op.second(s, inst)) {}   // hasta optimo local
            chequear(factible(s, inst), path + ": " + op.first + " factible");
            chequear(fabs(s.costo_total - evaluar(s, inst)) < EPS,
                     path + ": " + op.first + " costo incremental == evaluar");
            chequear(evaluar(s, inst) <= evaluar(vmc, inst) + EPS,
                     path + ": " + op.first + " no empeora");
        }

        // --- VND completo: factible y costo coherente ---
        Solucion v = vmc;
        metaheuristicas::vnd(v, inst);
        chequear(factible(v, inst), path + ": vnd factible");
        chequear(fabs(v.costo_total - evaluar(v, inst)) < EPS,
                 path + ": vnd costo coherente");
    }
    cout << "  " << g_pass << " PASS, " << g_fail << " FAIL\n";
}

// -----------------------------------------------------------------------------
//  E1 - Heuristicas constructivas (RQ1)
// -----------------------------------------------------------------------------

static void exp_e1() {
    cout << "== E1: constructivas ==\n";
    ofstream csv(out("e1_constructivas.csv"));
    csv << "instancia,grupo,n,"
        << "vmc_costo,vmc_sin_asignar,vmc_tiempo_s,"
        << "ins_costo,ins_sin_asignar,ins_tiempo_s\n";
    csv << fixed << setprecision(2);

    for (const auto& I : instancias()) {
        Instancia inst(I.path);
        Solucion vmc; double tv = cronometrar([&] { vmc = constructivas::vecino_mas_cercano(inst); });
        Solucion ins; double ti = cronometrar([&] { ins = constructivas::insercion(inst); });
        csv << I.nombre << "," << I.grupo << "," << I.n << ","
            << evaluar(vmc, inst) << "," << no_asignados(vmc) << "," << tv << ","
            << evaluar(ins, inst) << "," << no_asignados(ins) << "," << ti << "\n";
        cout << "  " << I.nombre << " vmc=" << evaluar(vmc, inst)
             << " ins=" << evaluar(ins, inst) << "\n";
    }
    cout << "  -> " << out("e1_constructivas.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  E2 - Operadores de busqueda local (RQ2)
// -----------------------------------------------------------------------------

static void exp_e2() {
    cout << "== E2: operadores de busqueda local ==\n";
    ofstream csv(out("e2_busqueda_local.csv"));
    csv << "instancia,grupo,constructiva,operador,"
        << "costo_inicial,costo_final,mejora_pct,tiempo_s,pasadas\n";
    csv << fixed << setprecision(4);

    // operadores generalizados (relocate/swap multiple): no tienen variante
    // first/best, aplican una estrategia fija. Para evaluarlos aislados, cada
    // "pasada" prueba q = 1..QMAX y aplica el primer (q, par) que mejore, igual
    // que en el VND (QMAX = 10).
    const int QMAX = 10;
    auto relocate_mult = [QMAX](Solucion& s, const Instancia& inst) -> bool {
        for (int q = 1; q <= QMAX; q++)
            if (busqueda_local::relocate_multiple(s, inst, q)) return true;
        return false;
    };
    auto swap_mult = [QMAX](Solucion& s, const Instancia& inst) -> bool {
        for (int q = 1; q <= QMAX; q++)
            if (busqueda_local::swap_multiple(s, inst, q)) return true;
        return false;
    };

    vector<pair<string, function<bool(Solucion&, const Instancia&)>>> ops = {
        {"relocate_first",    busqueda_local::relocate_first_improvement},
        {"relocate_best",     busqueda_local::relocate_best_improvement},
        {"swap_first",        busqueda_local::swap_first_improvement},
        {"swap_best",         busqueda_local::swap_best_improvement},
        {"relocate_multiple", relocate_mult},
        {"swap_multiple",     swap_mult},
    };

    for (const auto& I : instancias()) {
        Instancia inst(I.path);
        Solucion vmc = constructivas::vecino_mas_cercano(inst);
        Solucion ins = constructivas::insercion(inst);
        vector<pair<string, Solucion*>> bases = {{"vmc", &vmc}, {"insercion", &ins}};

        for (auto& base : bases) {
            double ini = evaluar(*base.second, inst);
            for (auto& op : ops) {
                Solucion s = *base.second;
                int pasadas = 0;
                double t = cronometrar([&] { while (op.second(s, inst)) pasadas++; });
                double fin = evaluar(s, inst);
                double mejora = ini > 0 ? (ini - fin) / ini * 100.0 : 0.0;
                csv << I.nombre << "," << I.grupo << "," << base.first << ","
                    << op.first << "," << ini << "," << fin << ","
                    << mejora << "," << t << "," << pasadas << "\n";
            }
        }
        cout << "  " << I.nombre << " listo\n";
    }
    cout << "  -> " << out("e2_busqueda_local.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  E3 - VND vs operadores individuales (RQ3)
// -----------------------------------------------------------------------------

// composiciones de busqueda local para comparar contra el VND completo
static void solo_relocate(Solucion& s, const Instancia& inst) {
    while (busqueda_local::relocate_first_improvement(s, inst)) {}
}
static void solo_swap(Solucion& s, const Instancia& inst) {
    while (busqueda_local::swap_first_improvement(s, inst)) {}
}
static void vnd_reloc_swap(Solucion& s, const Instancia& inst) {
    bool mejoro = true;
    while (mejoro) {
        mejoro = false;
        while (busqueda_local::relocate_first_improvement(s, inst)) mejoro = true;
        while (busqueda_local::swap_first_improvement(s, inst)) mejoro = true;
    }
}

static void exp_e3() {
    cout << "== E3: VND vs operadores individuales ==\n";
    ofstream csv(out("e3_vnd.csv"));
    csv << "instancia,grupo,metodo,costo_inicial,costo_final,mejora_pct,tiempo_s\n";
    csv << fixed << setprecision(4);

    vector<pair<string, function<void(Solucion&, const Instancia&)>>> metodos = {
        {"solo_relocate",      solo_relocate},
        {"solo_swap",          solo_swap},
        {"vnd_relocate_swap",  vnd_reloc_swap},
        {"vnd_completo",       [](Solucion& s, const Instancia& i){ metaheuristicas::vnd(s, i); }},
    };

    for (const auto& I : instancias()) {
        Instancia inst(I.path);
        // mejor de las dos constructivas como punto de partida
        Solucion vmc = constructivas::vecino_mas_cercano(inst);
        Solucion ins = constructivas::insercion(inst);
        Solucion base = (evaluar(ins, inst) < evaluar(vmc, inst)) ? ins : vmc;
        double ini = evaluar(base, inst);

        for (auto& m : metodos) {
            Solucion s = base;
            double t = cronometrar([&] { m.second(s, inst); });
            double fin = evaluar(s, inst);
            double mejora = ini > 0 ? (ini - fin) / ini * 100.0 : 0.0;
            csv << I.nombre << "," << I.grupo << "," << m.first << ","
                << ini << "," << fin << "," << mejora << "," << t << "\n";
        }
        cout << "  " << I.nombre << " listo\n";
    }
    cout << "  -> " << out("e3_vnd.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  E4 - Tuning de parametros (RQ4): diferencia entre parametros
// -----------------------------------------------------------------------------

static void exp_e4() {
    cout << "== E4: tuning de parametros (ILS/LNS) ==\n";
    ofstream csv(out("e4_tuning.csv"));
    csv << "instancia,grupo,metaheuristica,parametro,valor,segundos,semillas,"
        << "costo_mejor,costo_peor,costo_medio,desvio,tiempo_medio_s\n";
    csv << fixed << setprecision(2);

    // subconjunto representativo: uno por tier, incluyendo un caso ajustado (b/e)
    vector<InstInfo> sub = {
        {"instances/gap/gap_b/b10200", "b10200", 'b', 200},
        {"instances/gap/gap_e/e15900", "e15900", 'e', 900},
        {"instances/real/real_instance", "real", 'r', 1100},
    };
    vector<int> fuerzas = {5, 10, 20, 40};      // perturbacion de ILS
    vector<int> destruirs = {20, 50, 100};      // tamano de destruccion de LNS

    for (const auto& I : sub) {
        Instancia inst(I.path);
        double seg = tier_segundos(I.n);

        // --- ILS variando fuerza ---
        for (int f : fuerzas) {
            vector<double> costos, tiempos;
            for (unsigned s = 1; s <= SEMILLAS; s++) {
                Solucion sol;
                double t = cronometrar([&] { sol = metaheuristicas::ils(inst, 1000000000, f, s, seg); });
                costos.push_back(evaluar(sol, inst));
                tiempos.push_back(t);
            }
            Stats st = resumir(costos);
            double tm = accumulate(tiempos.begin(), tiempos.end(), 0.0) / tiempos.size();
            csv << I.nombre << "," << I.grupo << ",ILS,fuerza," << f << ","
                << seg << "," << SEMILLAS << ","
                << st.mejor << "," << st.peor << "," << st.media << ","
                << st.desvio << "," << tm << "\n";
            cout << "  " << I.nombre << " ILS fuerza=" << f
                 << " medio=" << st.media << "\n";
        }

        // --- LNS variando destruir ---
        for (int d : destruirs) {
            vector<double> costos, tiempos;
            for (unsigned s = 1; s <= SEMILLAS; s++) {
                Solucion sol;
                double t = cronometrar([&] { sol = metaheuristicas::lns(inst, 1000000000, d, s, seg); });
                costos.push_back(evaluar(sol, inst));
                tiempos.push_back(t);
            }
            Stats st = resumir(costos);
            double tm = accumulate(tiempos.begin(), tiempos.end(), 0.0) / tiempos.size();
            csv << I.nombre << "," << I.grupo << ",LNS,destruir," << d << ","
                << seg << "," << SEMILLAS << ","
                << st.mejor << "," << st.peor << "," << st.media << ","
                << st.desvio << "," << tm << "\n";
            cout << "  " << I.nombre << " LNS destruir=" << d
                 << " medio=" << st.media << "\n";
        }
    }
    cout << "  -> " << out("e4_tuning.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  E5 - Comparacion final (RQ5): constructiva -> VND -> ILS
// -----------------------------------------------------------------------------

static void exp_e5() {
    cout << "== E5: comparacion final ==\n";
    ofstream csv(out("e5_final.csv"));
    csv << "instancia,grupo,n,"
        << "constructiva_costo,vnd_costo,"
        << "ils_mejor,ils_peor,ils_medio,ils_desvio,ils_tiempo_medio_s,"
        << "lns_mejor,lns_peor,lns_medio,lns_desvio,lns_tiempo_medio_s,"
        << "mejor_encontrado,gap_constructiva_pct,gap_vnd_pct,gap_ils_pct,gap_lns_pct\n";
    csv << fixed << setprecision(2);

    for (const auto& I : instancias()) {
        Instancia inst(I.path);
        double seg = tier_segundos(I.n);

        Solucion vmc = constructivas::vecino_mas_cercano(inst);
        Solucion ins = constructivas::insercion(inst);
        Solucion base = (evaluar(ins, inst) < evaluar(vmc, inst)) ? ins : vmc;
        double c_constr = evaluar(base, inst);

        Solucion vnd = base;
        metaheuristicas::vnd(vnd, inst);
        double c_vnd = evaluar(vnd, inst);

        // ILS (fuerza por defecto) sobre SEMILLAS semillas
        vector<double> ils_costos, ils_tiempos;
        for (unsigned s = 1; s <= SEMILLAS; s++) {
            Solucion sol;
            double t = cronometrar([&] {
                sol = metaheuristicas::ils(inst, 1000000000, DEFAULT_FUERZA, s, seg);
            });
            ils_costos.push_back(evaluar(sol, inst));
            ils_tiempos.push_back(t);
        }
        Stats ils = resumir(ils_costos);
        double ils_tm = accumulate(ils_tiempos.begin(), ils_tiempos.end(), 0.0) / ils_tiempos.size();

        // LNS (destruir segun tamano) sobre SEMILLAS semillas
        int destruir = lns_destruir(I.n);
        vector<double> lns_costos, lns_tiempos;
        for (unsigned s = 1; s <= SEMILLAS; s++) {
            Solucion sol;
            double t = cronometrar([&] {
                sol = metaheuristicas::lns(inst, 1000000000, destruir, s, seg);
            });
            lns_costos.push_back(evaluar(sol, inst));
            lns_tiempos.push_back(t);
        }
        Stats lns = resumir(lns_costos);
        double lns_tm = accumulate(lns_tiempos.begin(), lns_tiempos.end(), 0.0) / lns_tiempos.size();

        double mejor = min({c_constr, c_vnd, ils.mejor, lns.mejor});
        auto gap = [&](double c) { return mejor > 0 ? (c - mejor) / mejor * 100.0 : 0.0; };

        csv << I.nombre << "," << I.grupo << "," << I.n << ","
            << c_constr << "," << c_vnd << ","
            << ils.mejor << "," << ils.peor << "," << ils.media << ","
            << ils.desvio << "," << ils_tm << ","
            << lns.mejor << "," << lns.peor << "," << lns.media << ","
            << lns.desvio << "," << lns_tm << ","
            << mejor << "," << gap(c_constr) << "," << gap(c_vnd) << ","
            << gap(ils.mejor) << "," << gap(lns.mejor) << "\n";
        cout << "  " << I.nombre << " constr=" << c_constr
             << " vnd=" << c_vnd << " ils=" << ils.mejor << " lns=" << lns.mejor << "\n";
    }
    cout << "  -> " << out("e5_final.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  E6 - Caso real (RQ6)
// -----------------------------------------------------------------------------

static void exp_e6() {
    cout << "== E6: caso real ==\n";
    ofstream csv(out("e6_real.csv"));
    csv << "metodo,semillas,costo_mejor,costo_peor,costo_medio,costo_desvio,"
        << "sin_asignar,distancia_media,tiempo_medio_s\n";
    csv << fixed << setprecision(2);

    Instancia inst("instances/real/real_instance");
    double seg = tier_segundos(1100);

    // fila para metodos deterministicos: 1 corrida, las estadisticas colapsan al
    // unico valor (desvio 0). sin_asignar/distancia_media salen de esa solucion.
    auto fila_det = [&](const string& nombre, const Solucion& s, double t) {
        double c = evaluar(s, inst);
        csv << nombre << ",1," << c << "," << c << "," << c << ",0.00,"
            << no_asignados(s) << "," << distancia_media(s, inst) << "," << t << "\n";
        cout << "  " << nombre << " costo=" << c
             << " sin_asignar=" << no_asignados(s) << "\n";
    };

    // fila para metaheuristicas: SEMILLAS corridas, reporta mejor/peor/medio/desvio
    // y tiempo medio; sin_asignar/distancia_media se toman de la mejor solucion.
    auto fila_mh = [&](const string& nombre,
                       const function<Solucion(unsigned)>& correr) {
        vector<double> costos, tiempos;
        Solucion mejor_sol; double mejor_costo = 1e300;
        for (unsigned s = 1; s <= SEMILLAS; s++) {
            Solucion sol;
            double t = cronometrar([&] { sol = correr(s); });
            double c = evaluar(sol, inst);
            costos.push_back(c);
            tiempos.push_back(t);
            if (c < mejor_costo) { mejor_costo = c; mejor_sol = sol; }
        }
        Stats st = resumir(costos);
        double tm = accumulate(tiempos.begin(), tiempos.end(), 0.0) / tiempos.size();
        csv << nombre << "," << SEMILLAS << ","
            << st.mejor << "," << st.peor << "," << st.media << "," << st.desvio << ","
            << no_asignados(mejor_sol) << "," << distancia_media(mejor_sol, inst) << ","
            << tm << "\n";
        cout << "  " << nombre << " mejor=" << st.mejor << " medio=" << st.media
             << " (" << SEMILLAS << " semillas)\n";
    };

    Solucion vmc = constructivas::vecino_mas_cercano(inst);
    Solucion ins = constructivas::insercion(inst);
    Solucion base = (evaluar(ins, inst) < evaluar(vmc, inst)) ? ins : vmc;
    fila_det("mejor_constructiva", base, 0.0);

    Solucion vnd = base;
    double tv = cronometrar([&] { metaheuristicas::vnd(vnd, inst); });
    fila_det("+VND", vnd, tv);

    fila_mh("+ILS", [&](unsigned s) {
        return metaheuristicas::ils(inst, 1000000000, DEFAULT_FUERZA, s, seg);
    });
    fila_mh("+LNS", [&](unsigned s) {
        return metaheuristicas::lns(inst, 1000000000, lns_destruir(1100), s, seg);
    });

    cout << "  -> " << out("e6_real.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  Convergencia: costo del incumbente vs presupuesto de tiempo (para fig:conv)
// -----------------------------------------------------------------------------

static void exp_convergencia() {
    cout << "== Convergencia: costo vs tiempo ==\n";
    ofstream csv(out("convergencia.csv"));
    csv << "instancia,metodo,segundos,costo\n";
    csv << fixed << setprecision(2);

    struct Caso { string path; string nombre; int n; vector<double> budgets; };
    vector<Caso> casos = {
        {"instances/gap/gap_e/e20200", "e20200", 200, {0.25, 0.5, 1, 2, 3, 5, 8, 10}},
        {"instances/real/real_instance", "real", 1100, {1, 2, 5, 10, 20, 30, 45, 60}},
    };

    for (auto& c : casos) {
        Instancia inst(c.path);
        int destruir = lns_destruir(c.n);
        for (double t : c.budgets) {
            // cada presupuesto reinicia la metaheuristica (semilla 1) y corta a 't' s
            Solucion si = metaheuristicas::ils(inst, 1000000000, DEFAULT_FUERZA, 1, t);
            csv << c.nombre << ",ILS," << t << "," << evaluar(si, inst) << "\n";
            Solucion sl = metaheuristicas::lns(inst, 1000000000, destruir, 1, t);
            csv << c.nombre << ",LNS," << t << "," << evaluar(sl, inst) << "\n";
        }
        cout << "  " << c.nombre << " listo\n";
    }
    cout << "  -> " << out("convergencia.csv") << "\n";
}

// -----------------------------------------------------------------------------
//  Instancias sinteticas: peor caso / mejor caso de cada tipo de metodo
// -----------------------------------------------------------------------------

// escribe una instancia en formato OR-Library (demanda d_j = misma por deposito)
static void escribir_instancia(const string& path, int m, int n,
                               const vector<vector<double>>& costo_mn,
                               const vector<int>& demanda,
                               const vector<int>& cap) {
    ofstream f(path);
    f << m << " " << n << "\n";
    for (int i = 0; i < m; i++) {              // matriz de costos m x n
        for (int j = 0; j < n; j++) f << costo_mn[i][j] << " ";
        f << "\n";
    }
    for (int i = 0; i < m; i++) {              // matriz de demanda m x n (d_ij = d_j)
        for (int j = 0; j < n; j++) f << demanda[j] << " ";
        f << "\n";
    }
    for (int i = 0; i < m; i++) f << cap[i] << " ";
    f << "\n";
}

static void test_sinteticas() {
    cout << "== Instancias sinteticas: peor/mejor caso ==\n";
    ofstream csv(out("sinteticas.csv"));
    csv << "caso,metodo,costo\n";
    csv << fixed << setprecision(2);
    string dir = string(OUTDIR) + "/sinteticas_tmp";
    crear_dir(dir);

    // ---------------------------------------------------------------------
    //  CASO 1 - constructivas: greedy miope (peor caso vecino_mas_cercano,
    //  mejor caso insercion/regret). m=2, n=2, todo demanda 1.
    //  v0: d0=1, d1=2 ;  v1: d0=1, d1=100. Capacidades 1 y 1.
    //  Greedy: v0->d0(1), v1->d0 lleno -> d1(100) = 101.
    //  Regret: prioriza v1 (regret 99) -> d0(1), luego v0->d1(2) = 3.
    // ---------------------------------------------------------------------
    {
        string p = dir + "/greedy_miope";
        // costo_mn[i][j]: deposito i, vendedor j
        escribir_instancia(p, 2, 2,
            {{1, 1}, {2, 100}},   // d0: v0=1,v1=1 ; d1: v0=2,v1=100
            {1, 1}, {1, 1});
        Instancia inst(p);
        double cv = evaluar(constructivas::vecino_mas_cercano(inst), inst);
        double ci = evaluar(constructivas::insercion(inst), inst);
        cout << "  [C1 greedy miope] vecino_mas_cercano=" << cv
             << "  insercion=" << ci << "\n";
        csv << "greedy_miope,vecino_mas_cercano," << cv << "\n";
        csv << "greedy_miope,insercion," << ci << "\n";
        chequear(cv > ci, "C1: greedy peor que regret (greedy miope)");
    }

    // ---------------------------------------------------------------------
    //  CASO 2 - constructivas: ambas optimas (mejor caso de ambas).
    //  Cada vendedor tiene un deposito claramente mas cercano con lugar.
    // ---------------------------------------------------------------------
    {
        string p = dir + "/ambas_optimas";
        escribir_instancia(p, 2, 2,
            {{1, 100}, {100, 1}},  // v0 prefiere d0, v1 prefiere d1
            {1, 1}, {1, 1});
        Instancia inst(p);
        double cv = evaluar(constructivas::vecino_mas_cercano(inst), inst);
        double ci = evaluar(constructivas::insercion(inst), inst);
        cout << "  [C2 ambas optimas] vecino_mas_cercano=" << cv
             << "  insercion=" << ci << "\n";
        csv << "ambas_optimas,vecino_mas_cercano," << cv << "\n";
        csv << "ambas_optimas,insercion," << ci << "\n";
        chequear(cv == ci && cv == 2, "C2: ambas constructivas optimas (=2)");
    }

    // ---------------------------------------------------------------------
    //  CASO 3 - busqueda local: optimo local del que relocate+swap NO escapan
    //  pero ciclo3/VND si. m=3, n=3, todas las capacidades=1 (depositos llenos).
    //  Asignacion diagonal v_i->d_i (costo 10 c/u = 30). Ningun swap mejora,
    //  relocate imposible (sin lugar). La rotacion v0->d1,v1->d2,v2->d0 = 3.
    //  Aristas "hacia adelante" = 1, "hacia atras" = 100, diagonal = 10.
    // ---------------------------------------------------------------------
    {
        string p = dir + "/ciclo3";
        // costo_mn[i][j]: deposito i (fila), vendedor j (col)
        // c[v][d]: diag=10; v0->d1=1,v1->d2=1,v2->d0=1; resto=100
        // -> por deposito: d0: v0=10, v1=100, v2=1
        //                  d1: v0=1,  v1=10,  v2=100
        //                  d2: v0=100,v1=1,   v2=10
        escribir_instancia(p, 3, 3,
            {{10, 100, 1}, {1, 10, 100}, {100, 1, 10}},
            {1, 1, 1}, {1, 1, 1});
        Instancia inst(p);

        // construir manualmente la asignacion diagonal (el optimo local trampa)
        Solucion s;
        s.vendedores_por_deposito.resize(3);
        s.deposito_asignado.assign(3, -1);
        s.asignar(0, 0); s.asignar(1, 1); s.asignar(2, 2);
        s.costo_total = evaluar(s, inst);
        double diag = s.costo_total;

        // relocate + swap: no pueden escapar
        Solucion bl = s;
        vnd_reloc_swap(bl, inst);
        double c_bl = evaluar(bl, inst);

        // VND completo (incluye ciclo3): escapa
        Solucion v = s;
        metaheuristicas::vnd(v, inst);
        double c_vnd = evaluar(v, inst);

        cout << "  [C3 ciclo3] diagonal=" << diag
             << "  relocate+swap=" << c_bl << "  vnd_completo=" << c_vnd << "\n";
        csv << "ciclo3,diagonal_inicial," << diag << "\n";
        csv << "ciclo3,relocate+swap," << c_bl << "\n";
        csv << "ciclo3,vnd_completo," << c_vnd << "\n";
        chequear(c_bl == diag, "C3: relocate+swap atascados en el optimo local");
        chequear(c_vnd < c_bl, "C3: VND (ciclo3) escapa del optimo local");
    }

    // ---------------------------------------------------------------------
    //  CASO 4 - metaheuristica: mejor caso (ILS recupera el optimo desde una
    //  mala constructiva) y peor caso (instancia ya optima: solo overhead).
    // ---------------------------------------------------------------------
    {
        // mejor caso: reusar greedy_miope (greedy=101, optimo=3)
        Instancia inst(dir + "/greedy_miope");
        double greedy = evaluar(constructivas::vecino_mas_cercano(inst), inst);
        Solucion ils = metaheuristicas::ils(inst, 200, 5, 1, 0);
        double c_ils = evaluar(ils, inst);
        cout << "  [C4 MH mejor caso] greedy=" << greedy << "  ILS=" << c_ils << "\n";
        csv << "mh_mejor_caso,greedy," << greedy << "\n";
        csv << "mh_mejor_caso,ILS," << c_ils << "\n";
        chequear(c_ils < greedy, "C4: ILS mejora mucho sobre la mala constructiva");

        // peor caso: instancia ya optima, ILS no puede mejorar (overhead puro)
        Instancia inst2(dir + "/ambas_optimas");
        double opt = evaluar(constructivas::vecino_mas_cercano(inst2), inst2);
        Solucion ils2 = metaheuristicas::ils(inst2, 200, 5, 1, 0);
        double c_ils2 = evaluar(ils2, inst2);
        cout << "  [C4 MH peor caso] optimo=" << opt << "  ILS=" << c_ils2 << "\n";
        csv << "mh_peor_caso,optimo," << opt << "\n";
        csv << "mh_peor_caso,ILS," << c_ils2 << "\n";
        chequear(c_ils2 == opt, "C4: ILS no mejora una instancia ya optima (overhead)");
    }

    cout << "  -> " << out("sinteticas.csv") << "\n";
    cout << "  " << g_pass << " PASS, " << g_fail << " FAIL (acumulado)\n";
}

// -----------------------------------------------------------------------------
//  main / dispatch
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    asegurar_outdir();
    string modo = (argc > 1) ? argv[1] : "all";

    if      (modo == "check")      { test_check(); }
    else if (modo == "e1")         { exp_e1(); }
    else if (modo == "e2")         { exp_e2(); }
    else if (modo == "e3")         { exp_e3(); }
    else if (modo == "e4")         { exp_e4(); }
    else if (modo == "e5")         { exp_e5(); }
    else if (modo == "e6")          { exp_e6(); }
    else if (modo == "convergencia") { exp_convergencia(); }
    else if (modo == "sinteticas")   { test_sinteticas(); }
    else if (modo == "all") {
        test_check();
        test_sinteticas();
        exp_e1();
        exp_e2();
        exp_e3();
        exp_e4();
        exp_e5();
        exp_e6();
        exp_convergencia();
    } else {
        cerr << "modo desconocido: " << modo << "\n"
             << "uso: ./tests [check|e1|e2|e3|e4|e5|e6|convergencia|sinteticas|all]\n";
        return 1;
    }

    if (g_fail > 0) {
        cerr << "\n*** " << g_fail << " test(s) fallaron ***\n";
        return 1;
    }
    return 0;
}
