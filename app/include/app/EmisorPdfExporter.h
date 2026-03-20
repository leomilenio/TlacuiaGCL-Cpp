#pragma once
#include <QList>
#include <QString>
#include "core/EmisorRepository.h"
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"
#include "core/LibreriaConfigRepository.h"

namespace App {

class EmisorPdfExporter {
public:
    static bool exportar(const Calculadora::EmisorRecord&            emisor,
                         const QList<Calculadora::ConcesionRecord>&  activas,
                         const QList<Calculadora::ConcesionRecord>&  finalizadas,
                         const QList<Calculadora::CorteResult>&      cortes,
                         const Calculadora::EmisorCorteResumen&      resumen,
                         const Calculadora::LibreriaConfig&          config,
                         const QString&                              folioDocumento,
                         const QString&                              filePath,
                         bool                                        includeFirmas = true);
};

} // namespace App
