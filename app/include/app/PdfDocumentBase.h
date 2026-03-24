#pragma once
#include <QString>
#include <QByteArray>
#include <QPainter>
#include <QPrinter>
#include <QRectF>
#include <QSizeF>
#include <QTextDocument>
#include "core/LibreriaConfigRepository.h"

// ---------------------------------------------------------------------------
// App::Pdf — utilidades compartidas por todos los exportadores PDF
// ---------------------------------------------------------------------------
namespace App::Pdf {

// Imagen BLOB como data URI base64; vacío si no hay datos.
QString imgBase64(const QByteArray& data, const QString& mime, int height);

// CSS compartido entre todos los documentos.
// baseFontPt: tamaño de body (10.5 para Corte, 10.0 para Emisor).
QString sharedCss(qreal baseFontPt = 10.5);

// Bloque HTML completo del encabezado estándar.
// folioLabel : texto en la caja de folio (ej: "1-200326-F")
// fecha      : ya formateada (ej: "Generado el: 23/03/2026")
QString buildHeaderHtml(const Calculadora::LibreriaConfig& config,
                        const QString& folioLabel,
                        const QString& fecha);

// Línea de contacto en texto plano para el pie (vacío si sin datos).
QString buildFooterContactLine(const Calculadora::LibreriaConfig& config);

// Altura del pie en device pixels según si hay contacto.
inline qreal footerHeight(qreal dpppt, bool hasContact) {
    return (hasContact ? 30.0 : 18.0) * dpppt;
}

// Dibuja el pie en cada página (QPainter, coordenadas device pixels).
void drawFooter(QPainter& painter,
                qreal lineY, qreal footerHPx, qreal lineGapPx, qreal pageWidthPx,
                qreal res,
                const QString& folio, const QString& fecha,
                int pageNum, int totalPages,
                const QString& contactLine);

// Renderiza doc→printer con pie en cada página.
// doc ya debe tener setPaintDevice+setHtml+setPageSize aplicados.
void renderPages(QTextDocument& doc, QPrinter& printer,
                 qreal footerHPx, qreal lineGapPx,
                 const QRectF& contentPx, const QSizeF& docSize,
                 const QString& folio, const QString& fecha,
                 const QString& contactLine);

} // namespace App::Pdf
