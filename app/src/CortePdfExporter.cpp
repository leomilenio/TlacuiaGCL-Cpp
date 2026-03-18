#include "app/CortePdfExporter.h"
#include <QTextDocument>
#include <QPrinter>
#include <QLocale>
#include <QDate>

namespace App {

bool CortePdfExporter::exportar(const Calculadora::ConcesionRecord&       concesion,
                                const QList<Calculadora::ProductoRecord>& productos,
                                const Calculadora::CorteResult&           corte,
                                const QString&                            filePath)
{
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v).toHtmlEscaped(); };
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };

    QString emisor = concesion.emisorNombre.isEmpty() ? "(Sin distribuidor)" : concesion.emisorNombre;
    QString folio  = concesion.folio.isEmpty()        ? "(Sin folio)"        : concesion.folio;
    QString fecha  = QDate::currentDate().toString("dd/MM/yyyy");

    // ---------------------------------------------------------------------------
    // HTML del documento
    // ---------------------------------------------------------------------------
    QString html;
    html.reserve(4096);

    html += R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body       { font-family: Arial, Helvetica, sans-serif; font-size: 11pt; color: #222; }
  h1         { font-size: 16pt; margin: 0 0 4px 0; }
  h2         { font-size: 12pt; margin: 0 0 2px 0; color: #555; }
  .meta      { margin-bottom: 12px; }
  .meta td   { padding: 2px 10px 2px 0; }
  table.productos { width: 100%; border-collapse: collapse; margin-top: 8px; }
  table.productos th {
      background: #2c3e50; color: white; padding: 5px 6px;
      text-align: left; font-size: 10pt;
  }
  table.productos td { padding: 4px 6px; font-size: 10pt; border-bottom: 1px solid #ddd; }
  table.productos tr:nth-child(even) td { background: #f5f5f5; }
  .totales   { margin-top: 14px; }
  .totales td { padding: 3px 10px; }
  .totales td.lbl { text-align: right; color: #555; }
  .totales td.val { font-weight: bold; text-align: right; }
  .fiscal    { margin-top: 8px; border-top: 1px solid #ccc; padding-top: 6px; }
  .firmas    { margin-top: 40px; }
  .firmas td { width: 40%; padding-top: 30px; border-top: 1px solid #333;
               text-align: center; font-size: 10pt; color: #555; }
  hr.sep     { border: none; border-top: 2px solid #2c3e50; margin: 10px 0; }
</style>
</head>
<body>
)";

    // Encabezado
    html += QString(R"(
<h1>Gestor de Concesiones Tlacuia</h1>
<hr class="sep">
<table class="meta">
  <tr><td><b>Distribuidor:</b></td><td>%1</td><td><b>Fecha:</b></td><td>%2</td></tr>
  <tr><td><b>Folio:</b></td><td>%3</td><td><b>Tipo:</b></td><td>%4</td></tr>
</table>
)").arg(esc(emisor), esc(fecha), esc(folio),
        concesion.tipoDocumento == Calculadora::TipoDocumentoConcesion::NotaDeCredito
            ? "Nota de credito" : "Factura");

    // Tabla de productos
    html += R"(
<table class="productos">
  <thead>
    <tr>
      <th>Descripcion</th><th>ISBN</th>
      <th style="text-align:center">Rec</th>
      <th style="text-align:center">Vend</th>
      <th style="text-align:right">Precio Neto</th>
      <th style="text-align:right">Subtotal Vend</th>
      <th style="text-align:center">Dev</th>
    </tr>
  </thead>
  <tbody>
)";

    for (const auto& p : productos) {
        int vendida  = p.cantidadVendida;
        int devuelta = p.cantidadRecibida - vendida;
        double subtotal = p.costo * vendida;
        html += QString(R"(
    <tr>
      <td>%1</td>
      <td>%2</td>
      <td style="text-align:center">%3</td>
      <td style="text-align:center">%4</td>
      <td style="text-align:right">%5</td>
      <td style="text-align:right">%6</td>
      <td style="text-align:center">%7</td>
    </tr>
)").arg(esc(p.nombreProducto), esc(p.isbn),
        QString::number(p.cantidadRecibida),
        QString::number(vendida),
        fmt(p.costo), fmt(subtotal),
        QString::number(devuelta));
    }

    html += "  </tbody>\n</table>\n";

    // Totales
    html += QString(R"(
<div class="totales">
<table style="margin-left:auto;">
  <tr><td class="lbl">Total a pagar al distribuidor:</td><td class="val">%1</td></tr>
  <tr><td class="lbl">Total devoluciones (piezas):</td><td class="val">%2</td></tr>
</table>
<div class="fiscal">
<table style="margin-left:auto;">
  <tr><td class="lbl">IVA Trasladado:</td><td class="val">%3</td></tr>
  <tr><td class="lbl">IVA Acreditable:</td><td class="val">%4</td></tr>
  <tr><td class="lbl">IVA Neto a SAT:</td><td class="val">%5</td></tr>
</table>
</div>
</div>
)").arg(fmt(corte.totalPagoAlDistribuidor),
        QString::number(corte.totalUnidadesDevueltas),
        fmt(corte.totalIvaTrasladado),
        fmt(corte.totalIvaAcreditable),
        fmt(corte.totalIvaNetoPagar));

    // Firmas
    html += R"(
<div class="firmas">
<table style="width:100%;">
  <tr>
    <td>Firma del encargado de libreria</td>
    <td></td>
    <td>Firma del distribuidor</td>
  </tr>
</table>
</div>
)";

    html += "</body></html>";

    // ---------------------------------------------------------------------------
    // Imprimir a PDF
    // ---------------------------------------------------------------------------
    QTextDocument doc;
    doc.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    doc.print(&printer);
    return true;
}

} // namespace App
