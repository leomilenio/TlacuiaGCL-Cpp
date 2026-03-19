#include "app/EmisorPdfExporter.h"
#include <QTextDocument>
#include <QPrinter>
#include <QLocale>
#include <QDate>

namespace App {

bool EmisorPdfExporter::exportar(
        const Calculadora::EmisorRecord&            emisor,
        const QList<Calculadora::ConcesionRecord>&  activas,
        const QList<Calculadora::ConcesionRecord>&  finalizadas,
        const QList<Calculadora::CorteResult>&      cortes,
        const Calculadora::EmisorCorteResumen&      resumen,
        const QString&                              filePath)
{
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v).toHtmlEscaped(); };
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };
    auto pct = [](double v) { return QString("%1%").arg(v, 0, 'f', 1); };

    QString fechaHoy = QDate::currentDate().toString("dd/MM/yyyy");
    QString nombre   = emisor.nombreEmisor.isEmpty() ? "(Sin nombre)" : emisor.nombreEmisor;

    QString html;
    html.reserve(8192);

    // ---- Estilos ----
    html += R"(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<style>
  body     { font-family: Arial, Helvetica, sans-serif; font-size: 10pt; color: #222; }
  h1       { font-size: 15pt; margin: 0 0 2px 0; }
  h2       { font-size: 11pt; margin: 14px 0 4px 0; color: #2c3e50;
             border-bottom: 1px solid #2c3e50; padding-bottom: 2px; }
  .meta td { padding: 2px 10px 2px 0; }
  table.t  { width: 100%; border-collapse: collapse; margin-top: 4px; font-size: 9pt; }
  table.t th { background:#2c3e50; color:#fff; padding:4px 6px; text-align:left; }
  table.t td { padding:3px 6px; border-bottom:1px solid #ddd; }
  table.t tr:nth-child(even) td { background:#f5f5f5; }
  .metrics  { margin: 6px 0; }
  .metrics td { padding: 3px 12px 3px 0; }
  .metrics td.lbl { color:#555; }
  .metrics td.val { font-weight:bold; }
  hr.sep   { border:none; border-top:2px solid #2c3e50; margin:8px 0; }
  .footer  { margin-top:40px; font-size:9pt; color:#777; }
  .footer td { width:45%; padding-top:24px; border-top:1px solid #333;
               text-align:center; }
  .red    { color:#D32F2F; }
  .orange { color:#E65100; }
  .green  { color:#2E7D32; }
</style>
</head><body>
)";

    // ---- Encabezado ----
    html += QString(R"(
<h1>Gestor de Concesiones Tlacuia</h1>
<hr class="sep">
<table class="meta">
  <tr>
    <td><b>Distribuidor:</b></td><td>%1</td>
    <td><b>Vendedor:</b></td><td>%2</td>
    <td><b>Fecha reporte:</b></td><td>%3</td>
  </tr>
</table>
)").arg(esc(nombre),
        esc(emisor.nombreVendedor),
        esc(fechaHoy));

    // ---- Sección 1: Datos del distribuidor ----
    html += "<h2>Datos del Distribuidor</h2>";
    html += "<table class='metrics'>";
    auto addMeta = [&](const QString& lbl, const QString& val) {
        html += QString("<tr><td class='lbl'>%1</td><td class='val'>%2</td></tr>")
                    .arg(esc(lbl), esc(val));
    };
    addMeta("Nombre:",      nombre);
    addMeta("Vendedor:",    emisor.nombreVendedor.isEmpty() ? "—" : emisor.nombreVendedor);
    addMeta("Telefono:",    emisor.telefono.isEmpty()       ? "—" : emisor.telefono);
    addMeta("Email:",       emisor.email.isEmpty()          ? "—" : emisor.email);
    addMeta("Facturación:", emisor.facturacion ? "Sí (emite CFDI)" : "No");
    html += "</table>";

    // ---- Sección 2: Concesiones activas ----
    html += QString("<h2>Concesiones Activas (%1)</h2>").arg(activas.size());
    if (activas.isEmpty()) {
        html += "<p style='color:#777;font-style:italic;'>Sin concesiones activas.</p>";
    } else {
        html += R"(<table class="t">
<thead><tr>
  <th>Folio</th><th>Tipo Doc.</th><th>Recepción</th><th>Vencimiento</th><th>Días rest.</th>
</tr></thead><tbody>)";
        for (const auto& c : activas) {
            int dias = c.diasRestantes();
            QString diasStr = dias >= 0 ? QString::number(dias)
                                        : QString("Vencida (%1)").arg(dias);
            QString cls = dias < 0 ? "red" : dias <= 14 ? "orange" : "green";
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                            "<td class='%5'><b>%6</b></td></tr>")
                        .arg(esc(c.folio.isEmpty() ? "(Sin folio)" : c.folio))
                        .arg(esc([&c](){
                                switch(c.tipoDocumento){
                                case Calculadora::TipoDocumentoConcesion::Factura:        return QString("Factura");
                                case Calculadora::TipoDocumentoConcesion::NotaDeCredito:  return QString("Nota de credito");
                                case Calculadora::TipoDocumentoConcesion::NotaDeRemision: return QString("Nota de remision");
                                default: return QString("Otro"); }}()))
                        .arg(esc(c.fechaRecepcion))
                        .arg(esc(c.fechaVencimiento))
                        .arg(cls, esc(diasStr));
        }
        html += "</tbody></table>";
    }

    // ---- Sección 3: Historial de cortes ----
    html += QString("<h2>Historial de Cortes (%1)</h2>").arg(finalizadas.size());
    if (finalizadas.isEmpty()) {
        html += "<p style='color:#777;font-style:italic;'>Sin concesiones finalizadas.</p>";
    } else {
        html += R"(<table class="t">
<thead><tr>
  <th>Folio</th><th>Fecha Cierre</th><th>Recibidas</th><th>Vendidas</th>
  <th>Devueltas</th><th>Total Ingresado</th><th>Total Devuelto</th><th>% Dev.</th>
</tr></thead><tbody>)";
        for (int i = 0; i < finalizadas.size(); ++i) {
            const auto& c  = finalizadas[i];
            const auto& ct = (i < cortes.size()) ? cortes[i] : Calculadora::CorteResult{};
            double pctDev = ct.totalUnidadesRecibidas > 0
                            ? ct.totalUnidadesDevueltas * 100.0 / ct.totalUnidadesRecibidas
                            : 0.0;
            QString cls = pctDev < 20 ? "green" : pctDev < 50 ? "orange" : "red";
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                            "<td>%5</td><td>%6</td><td>%7</td>"
                            "<td class='%8'><b>%9</b></td></tr>")
                        .arg(esc(c.folio.isEmpty() ? "(Sin folio)" : c.folio))
                        .arg(esc(c.fechaVencimiento))
                        .arg(ct.totalUnidadesRecibidas)
                        .arg(ct.totalUnidadesVendidas)
                        .arg(ct.totalUnidadesDevueltas)
                        .arg(fmt(ct.totalPrecioFinal))
                        .arg(fmt(ct.totalDevolucion))
                        .arg(cls)
                        .arg(esc(pct(pctDev)));
        }
        html += "</tbody></table>";
    }

    // ---- Sección 4: Métricas financieras ----
    html += "<h2>Métricas Financieras Consolidadas</h2>";
    html += "<table class='metrics'>";
    auto addMetric = [&](const QString& lbl, const QString& val) {
        html += QString("<tr><td class='lbl'>%1</td><td class='val'>%2</td></tr>")
                    .arg(esc(lbl), esc(val));
    };
    addMetric("Concesiones finalizadas:",       QString::number(resumen.totalConcesiones));
    addMetric("Unidades recibidas (total):",    QString::number(resumen.totalUnidadesRecibidas));
    addMetric("Unidades vendidas (total):",     QString::number(resumen.totalUnidadesVendidas));
    addMetric("Rotación promedio:",             pct(resumen.rotacionPromedio));
    addMetric("Tasa de devolución promedio:",   pct(resumen.tasaDevolucionPromedio));
    addMetric("Total acumulado ingresado:",     fmt(resumen.totalIngresado));
    addMetric("Total al distribuidor:",         fmt(resumen.totalAlDistribuidor));
    addMetric("Total comisiones (librería):",   fmt(resumen.totalComisiones));
    addMetric("Total devuelto (valor):",        fmt(resumen.totalDevolucion));
    html += "</table>";

    // ---- Pie de página ----
    html += R"(
<div class="footer">
<table style="width:100%;">
  <tr>
    <td>Encargado de librería</td>
    <td></td>
    <td>Representante del distribuidor</td>
  </tr>
</table>
</div>
)";
    html += "</body></html>";

    // ---- Imprimir a PDF ----
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
