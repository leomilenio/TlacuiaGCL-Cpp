#include "app/CortePdfExporter.h"
#include "app/PdfDocumentBase.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QPainter>
#include <QPrinter>
#include <QLocale>
#include <QDate>

namespace App {

bool CortePdfExporter::exportar(const Calculadora::ConcesionRecord&       concesion,
                                const QList<Calculadora::ProductoRecord>& productos,
                                const Calculadora::CorteResult&           corte,
                                const Calculadora::LibreriaConfig&        config,
                                const QString&                            folioDocumento,
                                const QString&                            filePath,
                                bool                                      includeFirmas)
{
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v).toHtmlEscaped(); };
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };

    const QString emisor  = concesion.emisorNombre.isEmpty() ? "(Sin distribuidor)" : concesion.emisorNombre;
    const QString fecha   = "Generado el: " + QDate::currentDate().toString("dd/MM/yyyy");
    const QString folio   = folioDocumento.isEmpty() ? "(sin folio)" : folioDocumento;

    // Tipo de documento
    QString tipoStr;
    switch (concesion.tipoDocumento) {
    case Calculadora::TipoDocumentoConcesion::Factura:        tipoStr = "Factura";           break;
    case Calculadora::TipoDocumentoConcesion::NotaDeCredito:  tipoStr = "Nota de crédito";   break;
    case Calculadora::TipoDocumentoConcesion::NotaDeRemision: tipoStr = "Nota de remisión";  break;
    default:                                                   tipoStr = "Otro";              break;
    }

    // CSS específico de Corte
    const QString cssPropio = R"(
  table.prod    { width:100%; margin-top:4px; }
  table.prod th { background:#1a3a5c; color:white; padding:5px 6px; font-size:9.5pt; }
  table.prod td { padding:4px 6px; font-size:9.5pt; border-bottom:1px solid #e0e0e0; }
  table.prod tr:nth-child(even) td { background:#f5f5f5; }
  .tot-lbl      { text-align:right; color:#555; padding:3px 8px; }
  .tot-val      { font-weight:bold; text-align:right; padding:3px 8px; }
  .fiscal       { border-top:1px solid #ccc; margin-top:6px; padding-top:6px; }
  hr.sep        { border:none; border-top:1px solid #ccc; margin:8px 0; }
)";

    QString html;
    html.reserve(8192);

    html += "<!DOCTYPE html>\n<html><head><meta charset=\"UTF-8\">\n<style>\n";
    html += App::Pdf::sharedCss(10.5);
    html += cssPropio;
    html += "</style></head><body>\n";

    html += App::Pdf::buildHeaderHtml(config, folio, fecha);

    // ---- Datos del corte ----
    html += QString(R"(
<table cellpadding="0" cellspacing="0" style="font-size:9.5pt; margin-bottom:8px;">
  <tr>
    <td style="padding-right:18px;"><b>Distribuidor:</b> %1</td>
    <td style="padding-right:18px;"><b>Folio de concesión:</b> %2</td>
    <td><b>Tipo:</b> %3</td>
  </tr>
</table>
<hr class="sep">
)").arg(esc(emisor), esc(concesion.folio.isEmpty() ? "(Sin folio)" : concesion.folio), esc(tipoStr));

    // ---- Tabla de productos ----
    html += R"(
<h2>Detalle de Productos</h2>
<table class="prod">
<thead><tr>
  <th>Descripción</th><th>ISBN</th>
  <th style="text-align:center">Rec.</th>
  <th style="text-align:center">Vend.</th>
  <th style="text-align:right">P. Neto</th>
  <th style="text-align:right">Subtotal</th>
  <th style="text-align:center">Dev.</th>
</tr></thead><tbody>
)";
    for (const auto& p : productos) {
        const int vendida  = p.cantidadVendida;
        const int devuelta = p.cantidadRecibida - vendida;
        html += QString(R"(<tr>
  <td>%1</td><td>%2</td>
  <td style="text-align:center">%3</td>
  <td style="text-align:center">%4</td>
  <td style="text-align:right">%5</td>
  <td style="text-align:right">%6</td>
  <td style="text-align:center">%7</td>
</tr>)").arg(esc(p.nombreProducto), esc(p.isbn),
             QString::number(p.cantidadRecibida), QString::number(vendida),
             fmt(p.costo), fmt(p.costo * vendida), QString::number(devuelta));
    }
    html += "</tbody></table>\n";

    // ---- Totales ----
    html += QString(R"(
<table style="margin-left:auto; margin-top:10px;">
  <tr><td class="tot-lbl">Total a pagar al distribuidor:</td><td class="tot-val">%1</td></tr>
  <tr><td class="tot-lbl">Total devoluciones (piezas):</td><td class="tot-val">%2</td></tr>
  <tr><td class="tot-lbl" style="color:#2E7D32;">Ganancia estimada (comisión):</td>
      <td class="tot-val" style="color:#2E7D32;">%3</td></tr>
</table>
<div class="fiscal">
<table style="margin-left:auto;">
  <tr><td class="tot-lbl">IVA Trasladado:</td><td class="tot-val">%4</td></tr>
  <tr><td class="tot-lbl">IVA Acreditable:</td><td class="tot-val">%5</td></tr>
  <tr><td class="tot-lbl">IVA Neto a SAT:</td><td class="tot-val">%6</td></tr>
</table>
</div>
)").arg(fmt(corte.totalPagoAlDistribuidor),
        QString::number(corte.totalUnidadesDevueltas),
        fmt(corte.gananciaEstimada),
        fmt(corte.totalIvaTrasladado),
        fmt(corte.totalIvaAcreditable),
        fmt(corte.totalIvaNetoPagar));

    // ---- Firmas (opcional) ----
    if (includeFirmas) {
        html += R"(
<table style="width:100%; margin-top:40px; border-collapse:collapse;" cellpadding="0" cellspacing="0">
  <tr>
    <td style="width:45%; text-align:center; border-top:1px solid #555;
               padding-top:10px; font-size:9pt; color:#555;">
      Firma del encargado de librería
    </td>
    <td style="width:10%;">&nbsp;</td>
    <td style="width:45%; text-align:center; border-top:1px solid #555;
               padding-top:10px; font-size:9pt; color:#555;">
      Firma del distribuidor
    </td>
  </tr>
</table>
)";
    }

    html += "</body></html>";

    // ---- Configurar impresora ----
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::Letter));
    printer.setPageMargins(QMarginsF(12.7, 12.7, 12.7, 12.7), QPageLayout::Millimeter);

    const qreal dpppt     = printer.resolution() / 72.0;
    const QString contact = App::Pdf::buildFooterContactLine(config);
    const qreal footerHPx = App::Pdf::footerHeight(dpppt, !contact.isEmpty());
    const qreal lineGapPx = 2.0 * dpppt;

    QRectF contentPx = printer.pageRect(QPrinter::DevicePixel);
    QSizeF docSize(contentPx.width(), contentPx.height() - footerHPx - lineGapPx);

    // COMPAT-09 / High-DPI: asociar el printer como paint device ANTES de setHtml() y
    // setPageSize(). Esto obliga a QTextDocument a calcular el layout en device pixels
    // usando el DPI del printer (1200 dpi) en lugar del DPI de la pantalla (96–144 dpi).
    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&printer);
    Q_ASSERT(doc.documentLayout()->paintDevice() != nullptr);
    doc.setDocumentMargin(0);
    doc.setHtml(html);
    doc.setPageSize(docSize);

    App::Pdf::renderPages(doc, printer, footerHPx, lineGapPx, contentPx, docSize,
                          folio, fecha, contact);
    return true;
}

} // namespace App
