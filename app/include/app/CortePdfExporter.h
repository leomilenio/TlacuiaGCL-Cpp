#pragma once
#include <QList>
#include <QString>
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"

namespace App {

// Genera un PDF con el resumen del corte de una concesion.
// Implementado con QTextDocument (HTML) + QPrinter::PdfFormat.
// No tiene dependencias externas mas alla de Qt6::PrintSupport.
class CortePdfExporter {
public:
    static bool exportar(const Calculadora::ConcesionRecord&        concesion,
                         const QList<Calculadora::ProductoRecord>&  productos,
                         const Calculadora::CorteResult&            corte,
                         const QString&                             filePath);
};

} // namespace App
