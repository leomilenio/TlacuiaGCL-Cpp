#include "app/EmisorPdfExporter.h"
#include "app/PdfDocumentBase.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QPainter>
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
        const Calculadora::LibreriaConfig&          config,
        const QString&                              folioDocumento,
        const QString&                              filePath,
        bool                                        includeFirmas)
{
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v).toHtmlEscaped(); };
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };
    auto pct = [](double v)   { return QString("%1%").arg(v, 0, 'f', 1); };

    const QString fechaHoy = "Generado el: " + QDate::currentDate().toString("dd/MM/yyyy");
    const QString nombre   = emisor.nombreEmisor.isEmpty() ? "(Sin nombre)" : emisor.nombreEmisor;
    const QString folio    = folioDocumento.isEmpty() ? "RI000-A" : folioDocumento;

    // Formatea fecha ISO a dd/MM/yy para celdas compactas
    auto shortDate = [](const QString& iso) -> QString {
        const QDate d = QDate::fromString(iso, Qt::ISODate);
        return d.isValid() ? d.toString("dd/MM/yy") : iso;
    };

    // Comisión promedio de todas las concesiones (activas + finalizadas)
    double comisionProm = 0.0;
    int numConc = activas.size() + finalizadas.size();
    if (numConc > 0) {
        for (const auto& c : activas)     comisionProm += c.comisionPct;
        for (const auto& c : finalizadas) comisionProm += c.comisionPct;
        comisionProm /= numConc;
    }

    // Totales de IVA acumulados desde los cortes (qty-weighted tras el fix de SQL)
    double ivaTrasladado  = 0.0;
    double ivaAcreditable = 0.0;
    double ivaNetoPagar   = 0.0;
    for (const auto& ct : cortes) {
        ivaTrasladado  += ct.totalIvaTrasladado;
        ivaAcreditable += ct.totalIvaAcreditable;
        ivaNetoPagar   += ct.totalIvaNetoPagar;
    }

    // CSS específico de Emisor
    const QString cssPropio = R"(
  table.t      { width:100%; margin-top:4px; font-size:9pt; }
  table.t th   { background:#1a3a5c; color:white; padding:4px 6px; text-align:left; }
  table.t td   { padding:3px 6px; border-bottom:1px solid #e0e0e0; }
  table.t tr:nth-child(even) td { background:#f5f5f5; }
  table.t-hist { width:100%; margin-top:4px; font-size:7.5pt; }
  table.t-hist th { background:#1a3a5c; color:white; padding:3px 4px; text-align:left; white-space:nowrap; }
  table.t-hist td { padding:2px 4px; border-bottom:1px solid #e0e0e0; white-space:nowrap; }
  table.t-hist tr:nth-child(even) td { background:#f5f5f5; }
  .met td    { padding:2px 10px 2px 0; font-size:9.5pt; }
  .met .lbl  { color:#555; }
  .met .val  { font-weight:bold; }
  .sec-lbl   { font-size:8.5pt; font-weight:bold; color:#1a3a5c; padding-top:6px !important;
               border-top:1px solid #d0d8e4; }
  .red       { color:#C62828; }
  .orange    { color:#E65100; }
  .green     { color:#2E7D32; }
)";

    QString html;
    html.reserve(12288);

    html += "<!DOCTYPE html>\n<html><head><meta charset=\"UTF-8\">\n<style>\n";
    html += App::Pdf::sharedCss(10.0);
    html += cssPropio;
    html += "</style></head><body>\n";

    html += App::Pdf::buildHeaderHtml(config, folio, fechaHoy);

    // ---- Datos del distribuidor ----
    html += QString(R"(
<h2>Reporte de Distribuidor</h2>
<table class="met">
  <tr><td class="lbl">Distribuidor:</td><td class="val">%1</td>
      <td style="width:20px;"></td>
      <td class="lbl">Vendedor:</td><td class="val">%2</td></tr>
  <tr><td class="lbl">Tel&eacute;fono:</td><td class="val">%3</td>
      <td></td>
      <td class="lbl">Email:</td><td class="val">%4</td></tr>
  <tr><td class="lbl">Facturaci&oacute;n:</td><td class="val">%5</td>
      <td></td>
      <td class="lbl">Comisi&oacute;n acordada:</td><td class="val">%6</td></tr>
</table>
)").arg(esc(nombre),
        esc(emisor.nombreVendedor.isEmpty() ? "—" : emisor.nombreVendedor),
        esc(emisor.telefono.isEmpty()        ? "—" : emisor.telefono),
        esc(emisor.email.isEmpty()           ? "—" : emisor.email),
        emisor.facturacion ? "S&iacute; (emite CFDI)" : "No",
        numConc > 0 ? pct(comisionProm) : "—");

    // ---- Concesiones activas ----
    html += QString("<h2>Concesiones Activas (%1)</h2>").arg(activas.size());
    if (activas.isEmpty()) {
        html += "<p style='color:#777;font-style:italic;'>Sin concesiones activas.</p>";
    } else {
        html += R"(<table class="t"><thead><tr>
  <th>Folio</th><th>Tipo Doc.</th><th>Recepción</th><th>Vencimiento</th><th>Días rest.</th>
</tr></thead><tbody>)";
        for (const auto& c : activas) {
            int dias = c.diasRestantes();
            QString cls = dias < 0 ? "red" : dias <= 14 ? "orange" : "green";
            QString diasStr = dias >= 0 ? QString::number(dias) : QString("Vencida (%1)").arg(dias);
            QString tipo;
            switch (c.tipoDocumento) {
            case Calculadora::TipoDocumentoConcesion::Factura:        tipo = "Factura"; break;
            case Calculadora::TipoDocumentoConcesion::NotaDeCredito:  tipo = "Nota de crédito"; break;
            case Calculadora::TipoDocumentoConcesion::NotaDeRemision: tipo = "Nota de remisión"; break;
            default: tipo = "Otro"; break;
            }
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                            "<td class='%5'><b>%6</b></td></tr>")
                    .arg(esc(c.folio.isEmpty() ? "(Sin folio)" : c.folio))
                    .arg(esc(tipo)).arg(esc(c.fechaRecepcion))
                    .arg(esc(c.fechaVencimiento)).arg(cls).arg(esc(diasStr));
        }
        html += "</tbody></table>";
    }

    // ---- Historial de cortes ----
    html += QString("<h2>Historial de Cortes (%1)</h2>").arg(finalizadas.size());
    if (finalizadas.isEmpty()) {
        html += "<p style='color:#777;font-style:italic;'>Sin concesiones finalizadas.</p>";
    } else {
        html += R"(<table class="t-hist"><thead><tr>
  <th>Folio</th><th>Periodo</th>
  <th>Rec.</th><th>Vend.</th><th>Dev.</th>
  <th>Total venta</th><th>Pago dist.</th>
  <th>Estado</th><th>% Dev.</th>
</tr></thead><tbody>)";
        for (int i = 0; i < finalizadas.size(); ++i) {
            const auto& c  = finalizadas[i];
            const auto& ct = (i < cortes.size()) ? cortes[i] : Calculadora::CorteResult{};
            double pctDev  = ct.totalUnidadesRecibidas > 0
                             ? ct.totalUnidadesDevueltas * 100.0 / ct.totalUnidadesRecibidas : 0.0;
            QString cls = pctDev < 20 ? "green" : pctDev < 50 ? "orange" : "red";
            const QString periodo = esc(shortDate(c.fechaRecepcion))
                                    + " &ndash; "
                                    + esc(shortDate(c.fechaVencimiento));
            html += QString("<tr>"
                            "<td>%1</td><td>%2</td>"
                            "<td style='text-align:center'>%3</td>"
                            "<td style='text-align:center'>%4</td>"
                            "<td style='text-align:center'>%5</td>"
                            "<td style='text-align:right'>%6</td>"
                            "<td style='text-align:right'>%7</td>"
                            "<td style='color:#555;'>Finalizada</td>"
                            "<td class='%8' style='text-align:center'><b>%9</b></td>"
                            "</tr>")
                    .arg(esc(c.folio.isEmpty() ? "(Sin folio)" : c.folio))
                    .arg(periodo)
                    .arg(ct.totalUnidadesRecibidas)
                    .arg(ct.totalUnidadesVendidas)
                    .arg(ct.totalUnidadesDevueltas)
                    .arg(fmt(ct.totalPrecioFinal))
                    .arg(fmt(ct.totalPagoAlDistribuidor))
                    .arg(cls).arg(esc(pct(pctDev)));
        }
        html += "</tbody></table>";
    }

    // ---- Métricas financieras ----
    html += R"(<h2>M&eacute;tricas Financieras Consolidadas</h2><table class="met">)";
    auto addMetric = [&](const QString& lbl, const QString& val, bool secHeader = false) {
        if (secHeader)
            html += QString("<tr><td colspan='2' class='sec-lbl'>%1</td></tr>").arg(esc(lbl));
        else
            html += QString("<tr><td class='lbl'>%1</td><td class='val'>%2</td></tr>")
                    .arg(esc(lbl), val);
    };

    // Inventario
    addMetric("— Inventario —", {}, true);
    addMetric("Concesiones finalizadas:",     QString::number(resumen.totalConcesiones));
    addMetric("Unidades recibidas (total):",  QString::number(resumen.totalUnidadesRecibidas));
    addMetric("Unidades vendidas (total):",   QString::number(resumen.totalUnidadesVendidas));
    addMetric("Unidades devueltas (total):",  QString::number(resumen.totalUnidadesDevueltas));
    addMetric("Rotaci\u00f3n promedio:",      pct(resumen.rotacionPromedio));
    addMetric("Tasa de devoluci\u00f3n promedio:", pct(resumen.tasaDevolucionPromedio));

    // Financiero
    addMetric("— Financiero —", {}, true);
    addMetric("Total de venta:",              fmt(resumen.totalIngresado));
    addMetric("Comisiones de librer\u00eda:", fmt(resumen.totalComisiones));
    addMetric("Total al distribuidor:",       fmt(resumen.totalAlDistribuidor));

    // Fiscal
    const QString ivaAcredLabel = emisor.facturacion
        ? "IVA Acreditable (CFDI proveedor):"
        : "IVA Acreditable (sin CFDI \u2014 no recuperable):";
    addMetric("— Fiscal —", {}, true);
    addMetric("IVA Trasladado (cobrado al cliente):", fmt(ivaTrasladado));
    addMetric(ivaAcredLabel,                          fmt(ivaAcreditable));
    addMetric("IVA Neto a enterar a SAT:",            fmt(ivaNetoPagar));

    html += "</table>";

    // ---- Firmas (opcional) ----
    if (includeFirmas) {
        html += R"(
<table style="width:100%; margin-top:40px; border-collapse:collapse;" cellpadding="0" cellspacing="0">
  <tr>
    <td style="width:45%; text-align:center; border-top:1px solid #555;
               padding-top:10px; font-size:9pt; color:#555;">
      Encargado de librería
    </td>
    <td style="width:10%;">&nbsp;</td>
    <td style="width:45%; text-align:center; border-top:1px solid #555;
               padding-top:10px; font-size:9pt; color:#555;">
      Representante del distribuidor
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

    // COMPAT-09 / High-DPI: ver comentario equivalente en CortePdfExporter.cpp.
    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&printer);
    Q_ASSERT(doc.documentLayout()->paintDevice() != nullptr);
    doc.setDocumentMargin(0);
    doc.setHtml(html);
    doc.setPageSize(docSize);

    App::Pdf::renderPages(doc, printer, footerHPx, lineGapPx, contentPx, docSize,
                          folio, fechaHoy, contact);
    return true;
}

} // namespace App
