#include "app/CortePdfExporter.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QPainter>
#include <QPrinter>
#include <QFontDatabase>
#include <QLocale>
#include <QDate>
#include <QCoreApplication>

namespace App {

// ---------------------------------------------------------------------------
// Helper: embeds a BLOB image as base64 data URI. Returns empty string if no data.
// ---------------------------------------------------------------------------
static QString imgBase64(const QByteArray& data, const QString& mime, int height) {
    if (data.isEmpty() || mime.isEmpty()) return {};
    return QString("<img src='data:%1;base64,%2' height='%3'>")
           .arg(mime, QString::fromLatin1(data.toBase64()), QString::number(height));
}

// ---------------------------------------------------------------------------
// drawFooter — dibuja el pie de página en coordenadas de device pixels
// ---------------------------------------------------------------------------
static void drawFooter(QPainter& painter,
                       qreal lineY, qreal footerHPx, qreal lineGapPx, qreal pageWidthPx,
                       qreal res,
                       const QString& folio, const QString& fecha,
                       int pageNum, int totalPages)
{
    // Línea separadora
    painter.setPen(QPen(QColor("#cccccc"), 1.0));
    painter.drawLine(QPointF(0, lineY), QPointF(pageWidthPx, lineY));

    // Texto del pie — fuente garantizada según SO
#if defined(Q_OS_WIN)
    QFont f("Segoe UI");          // Windows Vista+, siempre presente
#elif defined(Q_OS_MACOS)
    QFont f("Helvetica Neue");    // macOS 10.9+, siempre presente
#else
    QFont f("DejaVu Sans");       // Linux: paquete fonts-dejavu estándar
#endif
    f.setPixelSize(qRound(7.0 * res / 72.0));   // 7 pt en device pixels
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

// ---------------------------------------------------------------------------
// CortePdfExporter::exportar
// ---------------------------------------------------------------------------
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
    const QString fecha   = QDate::currentDate().toString("dd/MM/yyyy");
    const QString folio   = folioDocumento.isEmpty() ? "(sin folio)" : folioDocumento;

    // Tipo de documento
    QString tipoStr;
    switch (concesion.tipoDocumento) {
    case Calculadora::TipoDocumentoConcesion::Factura:        tipoStr = "Factura";           break;
    case Calculadora::TipoDocumentoConcesion::NotaDeCredito:  tipoStr = "Nota de crédito";   break;
    case Calculadora::TipoDocumentoConcesion::NotaDeRemision: tipoStr = "Nota de remisión";  break;
    default:                                                   tipoStr = "Otro";              break;
    }

    // Logos
    const QString logo1 = imgBase64(config.logoLibreria, config.logoLibreriaMime, 60);
    const QString logo2 = imgBase64(config.logoEmpresa,  config.logoEmpresaMime,  60);
    QString logosHtml = logo1 + ((!logo1.isEmpty() && !logo2.isEmpty()) ? "&nbsp;" : "") + logo2;
    if (logosHtml.isEmpty()) logosHtml = "<span style='font-size:9pt;color:#bbb;'>(Sin logo)</span>";

    // Tel
    QString tel = esc(config.tel1);
    if (!config.tel2.isEmpty()) tel += " / " + esc(config.tel2);

    // Empresa
    QString empresaLine = (!config.empresaNombre.isEmpty() && config.empresaNombre != config.libreriaNombre)
                          ? esc(config.empresaNombre) : esc(config.libreriaNombre);

    QString html;
    html.reserve(8192);

    // ---- CSS ----
    html += R"(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<style>
  body      { font-family: Arial, Helvetica, sans-serif; font-size: 10.5pt; color: #222; margin:0; padding:0; }
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
  table.prod    { width:100%; margin-top:4px; }
  table.prod th { background:#1a3a5c; color:white; padding:5px 6px; font-size:9.5pt; }
  table.prod td { padding:4px 6px; font-size:9.5pt; border-bottom:1px solid #e0e0e0; }
  table.prod tr:nth-child(even) td { background:#f5f5f5; }
  .tot-lbl      { text-align:right; color:#555; padding:3px 8px; }
  .tot-val      { font-weight:bold; text-align:right; padding:3px 8px; }
  .fiscal       { border-top:1px solid #ccc; margin-top:6px; padding-top:6px; }
  .firmas td    { width:40%; padding-top:28px; border-top:1px solid #555;
                  text-align:center; font-size:9pt; color:#555; }
  hr.sep        { border:none; border-top:1px solid #ccc; margin:8px 0; }
</style>
</head><body>
)";

    // ---- ENCABEZADO ----
    html += QString(R"(
<table class="hdr-main" cellpadding="0" cellspacing="0">
<tr>
  <td style="width:125px; text-align:center; padding-right:12px;">
    %1
  </td>
  <td style="vertical-align:top;">
    <table style="width:100%;" cellpadding="0" cellspacing="0"><tr>
      <td style="vertical-align:top;">
        <div class="libreria-name">%2</div>
        <table class="info-tbl" cellpadding="0" cellspacing="0">
          <tr>
            <td class="info-lbl">Empresa:</td><td style="padding-right:16px;">%3</td>
            <td class="info-lbl">RFC:</td><td>%4</td>
          </tr>
          <tr><td class="info-lbl">Tel:</td><td colspan="3">%5</td></tr>
        </table>
      </td>
      <td style="width:145px; vertical-align:middle; text-align:right;">
        <table class="folio-box" cellpadding="0" cellspacing="0" style="margin-left:auto;">
          <tr><td class="folio-hdr">FOLIO</td></tr>
          <tr><td class="folio-val">%6</td></tr>
        </table>
        <div class="fecha-lbl">%7</div>
      </td>
    </tr></table>
  </td>
</tr>
</table>
)").arg(logosHtml,
        esc(config.libreriaNombre.isEmpty() ? "Librería" : config.libreriaNombre),
        empresaLine, esc(config.rfc), tel, esc(folio), esc(fecha));

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

    const qreal res       = printer.resolution();          // device dots per inch
    const qreal dpppt     = res / 72.0;                    // device pixels per point
    const qreal footerHPx = 18.0 * dpppt;                  // 18 pt footer height
    const qreal lineGapPx =  2.0 * dpppt;                  // 2 pt gap

    QRectF contentPx = printer.pageRect(QPrinter::DevicePixel);
    QSizeF docSize(contentPx.width(), contentPx.height() - footerHPx - lineGapPx);

    // COMPAT-09 / High-DPI: asociar el printer como paint device ANTES de setHtml() y
    // setPageSize(). Esto obliga a QTextDocument a calcular el layout en device pixels
    // usando el DPI del printer (1200 dpi) en lugar del DPI de la pantalla (96–144 dpi).
    // Sin esta llamada, el contenido queda comprimido en la esquina superior izquierda
    // en pantallas Retina/HiDPI porque el layout usa coordenadas de pantalla mientras
    // que QPainter trabaja en coordenadas del printer.
    // Resultado: el pageCount() y las coordenadas son estables e independientes de la
    // escala o devicePixelRatio de la pantalla del usuario.
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

        // Contenido de la página i
        painter.save();
        painter.translate(0, -i * docSize.height());
        doc.drawContents(&painter, QRectF(0, i * docSize.height(),
                                          docSize.width(), docSize.height()));
        painter.restore();

        // Pie de página
        drawFooter(painter,
                   docSize.height() + lineGapPx, footerHPx, lineGapPx, contentPx.width(),
                   res, folio, fecha, i + 1, total);
    }
    painter.end();
    return true;
}

} // namespace App
