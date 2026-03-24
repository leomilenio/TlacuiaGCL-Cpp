#include "app/PdfDocumentBase.h"
#include <QCoreApplication>
#include <QPen>
#include <QFont>
#include <QColor>

namespace App::Pdf {

// ---------------------------------------------------------------------------
QString imgBase64(const QByteArray& data, const QString& mime, int height)
{
    if (data.isEmpty() || mime.isEmpty()) return {};
    return QString("<img src='data:%1;base64,%2' height='%3'>")
           .arg(mime, QString::fromLatin1(data.toBase64()), QString::number(height));
}

// ---------------------------------------------------------------------------
QString sharedCss(qreal baseFontPt)
{
    return QString(
        "  body      { font-family: Arial, Helvetica, sans-serif; font-size: %1pt; color: #222; margin:0; padding:0; }\n"
        "  table     { border-collapse: collapse; }\n"
        "  .hdr-main { width:100%; border-bottom: 2px solid #1a3a5c; margin-bottom:10px; }\n"
        "  .hdr-main td { padding-bottom:8px; vertical-align:middle; }\n"
        "  .libreria-name { font-size:13pt; font-weight:bold; color:#1a3a5c; margin-bottom:3px; }\n"
        "  .info-tbl td  { font-size:8.5pt; padding:1px 4px 1px 0; }\n"
        "  .info-lbl     { color:#555; font-weight:bold; }\n"
        "  .folio-box    { border:1.5px solid #1a3a5c; }\n"
        "  .folio-hdr    { background:#1a3a5c; color:white; font-size:6.5pt; font-weight:bold;\n"
        "                  text-align:center; padding:2px 10px; letter-spacing:1px; }\n"
        "  .folio-val    { font-size:9pt; font-weight:bold; color:#1a3a5c;\n"
        "                  text-align:center; padding:3px 12px; white-space:nowrap; }\n"
        "  .fecha-lbl    { font-size:7.5pt; color:#666; text-align:right; margin-top:3px; }\n"
        "  h2            { font-size:11pt; margin:12px 0 4px 0; color:#1a3a5c;\n"
        "                  border-bottom:1px solid #1a3a5c; padding-bottom:2px; }\n"
        "  .firmas td    { width:40%; padding-top:28px; border-top:1px solid #555;\n"
        "                  text-align:center; font-size:9pt; color:#555; }\n"
    ).arg(baseFontPt, 0, 'g');
}

// ---------------------------------------------------------------------------
QString buildHeaderHtml(const Calculadora::LibreriaConfig& config,
                        const QString& folioLabel,
                        const QString& fecha)
{
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };

    // Logos
    const QString logo1 = imgBase64(config.logoLibreria, config.logoLibreriaMime, 60);
    const QString logo2 = imgBase64(config.logoEmpresa,  config.logoEmpresaMime,  60);
    QString logosHtml = logo1 + ((!logo1.isEmpty() && !logo2.isEmpty()) ? "&nbsp;" : "") + logo2;
    if (logosHtml.isEmpty()) logosHtml = "<span style='font-size:9pt;color:#bbb;'>(Sin logo)</span>";

    // Nombre empresa (razón social, o librería si iguales)
    QString empresaLine = (!config.empresaNombre.isEmpty() && config.empresaNombre != config.libreriaNombre)
                          ? esc(config.empresaNombre) : esc(config.libreriaNombre);

    // RFC (solo el número, sin régimen)
    const QString rfcHtml = esc(config.rfc);

    // Régimen fiscal como fila independiente (opcional)
    QString regimenRow;
    if (!config.regimenFiscalCode.isEmpty()) {
        regimenRow = QString("<tr><td class=\"info-lbl\" style=\"white-space:nowrap;\">R&eacute;gimen:</td>"
                             "<td>%1 &ndash; %2</td></tr>\n")
                     .arg(esc(config.regimenFiscalCode), esc(config.regimenFiscalDesc));
    }

    // Fila de dirección (solo si hay datos)
    QString dirHtml;
    {
        QStringList partes;
        if (!config.dirCalle.isEmpty()) {
            QString num = config.dirCalle;
            if (!config.dirNumExterior.isEmpty()) num += " " + config.dirNumExterior;
            if (!config.dirNumInterior.isEmpty()) num += "-" + config.dirNumInterior;
            partes << esc(num);
        }
        if (!config.dirCodigoPostal.isEmpty()) partes << "C.P.&nbsp;" + esc(config.dirCodigoPostal);
        if (!config.dirColonia.isEmpty())      partes << "Col.&nbsp;" + esc(config.dirColonia);
        if (!config.dirMunicipio.isEmpty())    partes << esc(config.dirMunicipio);
        if (!config.dirEstado.isEmpty())       partes << esc(config.dirEstado);
        if (!partes.isEmpty())
            dirHtml = QString("<tr><td colspan=\"2\" style=\"font-size:8pt; color:#444;\">%1</td></tr>\n")
                      .arg(partes.join(",&nbsp; "));
    }

    // Filas de contacto en el encabezado (telefonos + email)
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
                                    "<td>%1</td></tr>\n").arg(telStr);
    if (!config.email.isEmpty())
        extraContactHtml += QString("<tr><td class=\"info-lbl\">Email:</td>"
                                    "<td>%1</td></tr>\n").arg(esc(config.email));

    return QString(R"(
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
            <td class="info-lbl" style="white-space:nowrap;">Empresa:</td>
            <td>%3</td>
          </tr>
          <tr>
            <td class="info-lbl">RFC:</td>
            <td>%4</td>
          </tr>
          %5
          %6
          %7
        </table>
      </td>
      <td style="width:165px; vertical-align:middle; text-align:right;">
        <table class="folio-box" cellpadding="0" cellspacing="0" style="width:100%;">
          <tr><td class="folio-hdr">FOLIO</td></tr>
          <tr><td class="folio-val">%8</td></tr>
        </table>
        <div class="fecha-lbl">%9</div>
      </td>
    </tr></table>
  </td>
</tr>
</table>
)").arg(logosHtml,
        esc(config.libreriaNombre.isEmpty() ? "Librería" : config.libreriaNombre),
        empresaLine, rfcHtml,
        regimenRow, dirHtml, extraContactHtml,
        esc(folioLabel), esc(fecha));
}

// ---------------------------------------------------------------------------
QString buildFooterContactLine(const Calculadora::LibreriaConfig& config)
{
    QString line;
    if (!config.contactoNombre.isEmpty())
        line = config.contactoNombre;

    QStringList tels;
    for (const auto& t : config.telefonos) {
        if (t.numero.isEmpty()) continue;
        QString s = t.numero;
        if (!t.tipo.isEmpty() && t.tipo != "Otro") s += " (" + t.tipo + ")";
        tels << s;
    }
    if (!tels.isEmpty()) {
        if (!line.isEmpty()) line += "   ";
        line += "Tel. " + tels.join("  /  ");
    }
    if (!config.email.isEmpty()) {
        if (!line.isEmpty()) line += "   ";
        line += "Correo: " + config.email;
    }
    return line;
}

// ---------------------------------------------------------------------------
void drawFooter(QPainter& painter,
                qreal lineY, qreal footerHPx, qreal lineGapPx, qreal pageWidthPx,
                qreal res,
                const QString& folio, const QString& fecha,
                int pageNum, int totalPages,
                const QString& contactLine)
{
    Q_UNUSED(footerHPx)

    // Línea separadora
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

    const qreal rowH = 11.0 * res / 72.0;
    qreal y = lineY + lineGapPx;

    // Fila de contacto (solo si hay datos)
    if (!contactLine.isEmpty()) {
        QRectF rc(0, y, pageWidthPx, rowH);
        painter.drawText(rc, Qt::AlignTop | Qt::AlignLeft, contactLine);
        y += rowH + 1.5 * res / 72.0;
    }

    // Fila del sistema: izquierda = app/versión, derecha = folio/fecha/página
    QRectF rs(0, y, pageWidthPx, rowH);
    const QString left  = QString("TlacuiaGCL - Gestor de Concesiones para Librerías | Versión %1")
                          .arg(QCoreApplication::applicationVersion());
    const QString right = QString("Folio: %1  |  %2  |  Pág. %3 de %4")
                          .arg(folio, fecha, QString::number(pageNum), QString::number(totalPages));
    painter.drawText(rs, Qt::AlignTop | Qt::AlignLeft,  left);
    painter.drawText(rs, Qt::AlignTop | Qt::AlignRight, right);
}

// ---------------------------------------------------------------------------
void renderPages(QTextDocument& doc, QPrinter& printer,
                 qreal footerHPx, qreal lineGapPx,
                 const QRectF& contentPx, const QSizeF& docSize,
                 const QString& folio, const QString& fecha,
                 const QString& contactLine)
{
    const qreal res   = printer.resolution();
    const int   total = doc.pageCount();

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
                   res, folio, fecha, i + 1, total, contactLine);
    }
    painter.end();
}

} // namespace App::Pdf
