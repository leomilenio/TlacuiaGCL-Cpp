#pragma once
#include "core/EmisorRepository.h"
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"
#include <QString>
#include <QList>

namespace App {

class EmisorPdfExporter {
public:
    static bool exportar(
        const Calculadora::EmisorRecord&            emisor,
        const QList<Calculadora::ConcesionRecord>&  activas,
        const QList<Calculadora::ConcesionRecord>&  finalizadas,
        const QList<Calculadora::CorteResult>&      cortes,
        const Calculadora::EmisorCorteResumen&      resumen,
        const QString&                              filePath);
};

} // namespace App
