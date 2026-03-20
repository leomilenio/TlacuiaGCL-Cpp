#pragma once
#include <QList>
#include <QString>
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"
#include "core/LibreriaConfigRepository.h"

namespace App {

class CortePdfExporter {
public:
    static bool exportar(const Calculadora::ConcesionRecord&        concesion,
                         const QList<Calculadora::ProductoRecord>&  productos,
                         const Calculadora::CorteResult&            corte,
                         const Calculadora::LibreriaConfig&         config,
                         const QString&                             folioDocumento,
                         const QString&                             filePath,
                         bool                                       includeFirmas = true);
};

} // namespace App
