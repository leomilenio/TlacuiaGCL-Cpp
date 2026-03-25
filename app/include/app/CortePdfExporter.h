#pragma once
#include <QList>
#include <QString>
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"
#include "core/LibreriaConfigRepository.h"

namespace App {

class CortePdfExporter {
public:
    // Corte Interno (CI) — uso exclusivo de la librería: ganancia, IVA fiscal completo.
    // preview=true: genera PDF con marca de agua "PRE-CORTE".
    static bool exportar(const Calculadora::ConcesionRecord&        concesion,
                         const QList<Calculadora::ProductoRecord>&  productos,
                         const Calculadora::CorteResult&            corte,
                         const Calculadora::LibreriaConfig&         config,
                         const QString&                             folioDocumento,
                         const QString&                             filePath,
                         bool                                       includeFirmas = true,
                         bool                                       preview       = false);

    // Corte Proveedor — se entrega al distribuidor: solo ventas/pago/devoluciones.
    // IVA incluido solo si concesion.emisorFacturacion == true.
    // preview=true: genera PDF con marca de agua "PRE-CORTE".
    static bool exportarProveedor(const Calculadora::ConcesionRecord&        concesion,
                                  const QList<Calculadora::ProductoRecord>&  productos,
                                  const Calculadora::CorteResult&            corte,
                                  const Calculadora::LibreriaConfig&         config,
                                  const QString&                             folioDocumento,
                                  const QString&                             filePath,
                                  bool                                       includeFirmas = true,
                                  bool                                       preview       = false);
};

} // namespace App
