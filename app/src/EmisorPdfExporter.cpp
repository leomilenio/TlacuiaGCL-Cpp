#include "app/EmisorPdfExporter.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QPainter>
#include <QPrinter>
#include <QFontDatabase>
#include <QLocale>
#include <QDate>
#include <QCoreApplication>

namespace App {

static QString imgBase64(const QByteArray& data, const QString& mime, int height) {
    if (data.isEmpty() || mime.isEmpty()) return {};
    return QString("<img src='data:%1;base64,%2' height='%3'>")
           .arg(mime, QString::fromLatin1(data.toBase64()), QString::number(height));
}

static void drawFooter(QPainter& painter,
                       qreal lineY, qreal footerHPx, qreal lineGapPx, qreal pageWidthPx,
                       qreal res,
                       const QString& folio, const QString& fecha,
                       int pageNum, int totalPages)
{
    painter.setPen(QPen(QColor("#cccccc"), 1.0));
    painter.drawLine(QPointF(0, lineY), QPointF(pageWidthPx, lineY));

#if defined(Q_OS_WIN)
    QFont f("Segoe UI");
#elif defined(Q_OS_MACOS)
    QFont f("Helvetica Neue");
#else
    QFont f("DejaVu Sans");
#endif
    f.setPixelSize(qRound(7.0 * res / 72.0));
    painter.setFont(f);
    painter.setPen(QColor("#888888"));

    QRectF r(0, lineY + lineGapPx, pageWidthPx, footerHPx - lineGapPx);

    const QString left  = QString("TlacuiaGCL - Gestor de Concesiones para Librerías | Versión %1")
                          .arg(QCoreApplication::applicationVersion());
    const QString right = QString("Folio: %1  |  %2  |  Pág. %3 de %4")
                          .arg(folio, fecha, QString::number(pageNum), QString::number(totalPages));

    painter.drawText(r, Qt::AlignTop | Qt::AlignLeft,  left);
    painter.drawText(r, Qt::AlignTop | Qt::AlignRight, right);
}

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

    const QString fechaHoy = QDate::currentDate().toString("dd/MM/yyyy");
    const QString nombre   = emisor.nombreEmisor.isEmpty() ? "(Sin nombre)" : emisor.nombreEmisor;
    const QString folio    = folioDocumento.isEmpty() ? "RI000-A" : folioDocumento;

    const QString logo1 = imgBase64(config.logoLibreria, config.logoLibreriaMime, 60);
    const QString logo2 = imgBase64(config.logoEmpresa,  config.logoEmpresaMime,  60);
    QString logosHtml = logo1 + ((!logo1.isEmpty() && !logo2.isEmpty()) ? "&nbsp;" : "") + logo2;
    if (logosHtml.isEmpty()) logosHtml = "<span style='font-size:9pt;color:#bbb;'>(Sin logo)</span>";

    // Construir filas adicionales de contacto (telefonos + email)
    QString telStr;
    for (const auto& t : config.telefonos) {
        if (t.numero.isEmpty()) continue;
        if (!telStr.isEmpty()) telStr += " &nbsp;·&nbsp; ";
        if (!t.tipo.isEmpty() && t.tipo != "Otro")
            telStr += "<b>" + esc(t.tipo) + ":</b>&nbsp;";
        telStr += esc(t.numero);
    }
    QString extraContactHtml;
    if (!telStr.isEmpty())
        extraContactHtml += QString("<tr><td class=\"info-lbl\">Tel:</td>"
                                    "<td colspan=\"3\">%1</td></tr>\n").arg(telStr);
    if (!config.email.isEmpty())
        extraContactHtml += QString("<tr><td class=\"info-lbl\">Email:</td>"
                                    "<td colspan=\"3\">%1</td></tr>\n").arg(esc(config.email));

    // Fila de regimen fiscal (solo si esta configurado)
    QString regimenHtml;
    if (!config.regimenFiscalCode.isEmpty())
        regimenHtml = QString("<tr><td class=\"info-lbl\" style=\"color:#777;font-size:7.5pt;\">Régimen:</td>"
                              "<td colspan=\"3\" style=\"color:#777;font-size:7.5pt;\">%1 – %2</td></tr>\n")
                      .arg(esc(config.regimenFiscalCode), esc(config.regimenFiscalDesc));

    QString empresaLine = (config.empresaNombre.isEmpty() || config.empresaNombre == config.libreriaNombre)
                          ? esc(config.libreriaNombre) : esc(config.empresaNombre);

    QString html;
    html.reserve(12288);

    html += R"(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<style>
  body      { font-family: Arial, Helvetica, sans-serif; font-size: 10pt; color: #222; margin:0; padding:0; }
  table     { border-collapse: collapse; }
  .hdr-main { width:100%; border-bottom: 2px solid #1a3a5c; margin-bottom:10px; }
  .hdr-main td { padding-bottom:8px; vertical-align:middle; }
  .libreria-name { font-size:13pt; font-weight:bold; color:#1a3a5c; margin-bottom:3px; }
  .info-tbl td  { font-size:8.5pt; padding:1px 4px 1px 0; }
  .info-lbl     { color:#555; font-weight:bold; }
  .folio-box    { border:1.5px solid #1a3a5c; }
  .folio-hdr    { background:#1a3a5c; color:white; font-size:6.5pt; font-weight:bold;
                  text-align:center; padding:2px 10px; letter-spacing:1px; }
  .folio-val    { font-size:9pt; font-weight:bold; color:#1a3a5c;
                  text-align:center; padding:3px 12px; white-space:nowrap; }
  .fecha-lbl    { font-size:7.5pt; color:#666; text-align:right; margin-top:3px; }
  h2            { font-size:11pt; margin:12px 0 4px 0; color:#1a3a5c;
                  border-bottom:1px solid #1a3a5c; padding-bottom:2px; }
  table.t    { width:100%; margin-top:4px; font-size:9pt; }
  table.t th { background:#1a3a5c; color:white; padding:4px 6px; text-align:left; }
  table.t td { padding:3px 6px; border-bottom:1px solid #e0e0e0; }
  table.t tr:nth-child(even) td { background:#f5f5f5; }
  .met td    { padding:2px 10px 2px 0; font-size:9.5pt; }
  .met .lbl  { color:#555; }
  .met .val  { font-weight:bold; }
  .red       { color:#C62828; }
  .orange    { color:#E65100; }
  .green     { color:#2E7D32; }
  .firmas td { width:40%; padding-top:28px; border-top:1px solid #555;
               text-align:center; font-size:9pt; color:#555; }
</style>
</head><body>
)";

    // ---- ENCABEZADO ----
    html += QString(R"(
<table class="hdr-main" cellpadding="0" cellspacing="0">
<tr>
  <td style="width:125px; text-align:center; padding-right:12px;">%1</td>
  <td style="vertical-align:top;">
    <table style="width:100%;" cellpadding="0" cellspacing="0"><tr>
      <td style="vertical-align:top;">
        <div class="libreria-name">%2</div>
        <table class="info-tbl" cellpadding="0" cellspacing="0">
          <tr>
            <td class="info-lbl">Empresa:</td><td style="padding-right:16px;">%3</td>
            <td class="info-lbl">RFC:</td><td>%4</td>
          </tr>
          %5
          %6
        </table>
      </td>
      <td style="width:145px; vertical-align:middle; text-align:right;">
        <table class="folio-box" cellpadding="0" cellspacing="0" style="margin-left:auto;">
          <tr><td class="folio-hdr">FOLIO</td></tr>
          <tr><td class="folio-val">%7</td></tr>
        </table>
        <div class="fecha-lbl">%8</div>
      </td>
    </tr></table>
  </td>
</tr>
</table>
)").arg(logosHtml,
        esc(config.libreriaNombre.isEmpty() ? "Librería" : config.libreriaNombre),
        empresaLine, esc(config.rfc),
        regimenHtml, extraContactHtml,
        esc(folio), esc(fechaHoy));

    // ---- Datos del distribuidor ----
    html += QString(R"(
<h2>Reporte de Distribuidor</h2>
<table class="met">
  <tr><td class="lbl">Distribuidor:</td><td class="val">%1</td>
      <td style="width:20px;"></td>
      <td class="lbl">Vendedor:</td><td class="val">%2</td></tr>
  <tr><td class="lbl">Teléfono:</td><td class="val">%3</td>
      <td></td>
      <td class="lbl">Email:</td><td class="val">%4</td></tr>
  <tr><td class="lbl">Facturación:</td><td class="val">%5</td></tr>
</table>
)").arg(esc(nombre),
        esc(emisor.nombreVendedor.isEmpty() ? "—" : emisor.nombreVendedor),
        esc(emisor.telefono.isEmpty()        ? "—" : emisor.telefono),
        esc(emisor.email.isEmpty()           ? "—" : emisor.email),
        emisor.facturacion ? "Sí (emite CFDI)" : "No");

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
        html += R"(<table class="t"><thead><tr>
  <th>Folio</th><th>Fecha Cierre</th><th>Recibidas</th><th>Vendidas</th>
  <th>Devueltas</th><th>Total Ingresado</th><th>Total Devuelto</th><th>% Dev.</th>
</tr></thead><tbody>)";
        for (int i = 0; i < finalizadas.size(); ++i) {
            const auto& c  = finalizadas[i];
            const auto& ct = (i < cortes.size()) ? cortes[i] : Calculadora::CorteResult{};
            double pctDev  = ct.totalUnidadesRecibidas > 0
                             ? ct.totalUnidadesDevueltas * 100.0 / ct.totalUnidadesRecibidas : 0.0;
            QString cls = pctDev < 20 ? "green" : pctDev < 50 ? "orange" : "red";
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                            "<td>%5</td><td>%6</td><td>%7</td>"
                            "<td class='%8'><b>%9</b></td></tr>")
                    .arg(esc(c.folio.isEmpty() ? "(Sin folio)" : c.folio))
                    .arg(esc(c.fechaVencimiento))
                    .arg(ct.totalUnidadesRecibidas).arg(ct.totalUnidadesVendidas)
                    .arg(ct.totalUnidadesDevueltas)
                    .arg(fmt(ct.totalPrecioFinal)).arg(fmt(ct.totalDevolucion))
                    .arg(cls).arg(esc(pct(pctDev)));
        }
        html += "</tbody></table>";
    }

    // ---- Métricas financieras ----
    html += R"(<h2>Métricas Financieras Consolidadas</h2><table class="met">)";
    auto addMetric = [&](const QString& lbl, const QString& val) {
        html += QString("<tr><td class='lbl'>%1</td><td class='val'>%2</td></tr>")
                .arg(esc(lbl), esc(val));
    };
    addMetric("Concesiones finalizadas:",     QString::number(resumen.totalConcesiones));
    addMetric("Unidades recibidas (total):",  QString::number(resumen.totalUnidadesRecibidas));
    addMetric("Unidades vendidas (total):",   QString::number(resumen.totalUnidadesVendidas));
    addMetric("Rotación promedio:",           pct(resumen.rotacionPromedio));
    addMetric("Tasa de devolución promedio:", pct(resumen.tasaDevolucionPromedio));
    addMetric("Total acumulado ingresado:",   fmt(resumen.totalIngresado));
    addMetric("Total al distribuidor:",       fmt(resumen.totalAlDistribuidor));
    addMetric("Total comisiones (librería):", fmt(resumen.totalComisiones));
    addMetric("Total devuelto (valor):",      fmt(resumen.totalDevolucion));
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

    const qreal res       = printer.resolution();
    const qreal dpppt     = res / 72.0;
    const qreal footerHPx = 18.0 * dpppt;
    const qreal lineGapPx =  2.0 * dpppt;

    QRectF contentPx = printer.pageRect(QPrinter::DevicePixel);
    QSizeF docSize(contentPx.width(), contentPx.height() - footerHPx - lineGapPx);

    // COMPAT-09 / High-DPI: ver comentario equivalente en CortePdfExporter.cpp.
    // setPaintDevice antes de setHtml/setPageSize garantiza layout en device pixels
    // del printer (1200 dpi), independiente del devicePixelRatio de la pantalla.
    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&printer);
    Q_ASSERT(doc.documentLayout()->paintDevice() != nullptr);
    doc.setDocumentMargin(0);
    doc.setHtml(html);
    doc.setPageSize(docSize);

    const int total = doc.pageCount();

    QPainter painter(&printer);
    for (int i = 0; i < total; ++i) {
        if (i > 0) printer.newPage();

        painter.save();
        painter.translate(0, -i * docSize.height());
        doc.drawContents(&painter, QRectF(0, i * docSize.height(),
                                          docSize.width(), docSize.height()));
        painter.restore();

        drawFooter(painter,
                   docSize.height() + lineGapPx, footerHPx, lineGapPx, contentPx.width(),
                   res, folio, fechaHoy, i + 1, total);
    }
    painter.end();
    return true;
}

} // namespace App
