#include "core/FolioRepository.h"
#include "core/DatabaseManager.h"
#include "core/ConcesionRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>

namespace Calculadora {

FolioRepository::FolioRepository(DatabaseManager& dbManager)
    : m_db(dbManager) {}

// Formato corte: {numero}-{ddMMyy}-{F|N|R|O}
// Ejemplos: 0-190326-F (primera factura del 19/03/26)
static QString formatearFolioCorte(int numero, const QDate& fecha,
                                    TipoDocumentoConcesion tipo) {
    char letra = 'O';
    switch (tipo) {
    case TipoDocumentoConcesion::Factura:        letra = 'F'; break;
    case TipoDocumentoConcesion::NotaDeCredito:  letra = 'N'; break;
    case TipoDocumentoConcesion::NotaDeRemision: letra = 'R'; break;
    default: break;
    }
    return QString("%1-%2-%3")
           .arg(numero)
           .arg(fecha.toString("ddMMyy"))
           .arg(QLatin1Char(letra));
}

// Formato RI: RI{NNN}-{A-Z}
// Ejemplos: RI000-A (primero), RI999-A (1000º), RI000-B (1001º)
static QString formatearFolioRI(int contador) {
    int  num      = contador % 1000;
    int  letraIdx = contador / 1000;
    char letra    = (letraIdx < 26) ? static_cast<char>('A' + letraIdx) : 'Z';
    return QString("RI%1-%2").arg(num, 3, 10, QLatin1Char('0')).arg(QLatin1Char(letra));
}

QString FolioRepository::getFolioCorte(int64_t concesionId,
                                        TipoDocumentoConcesion tipo) {
    QSqlQuery q(m_db.database());

    // 1. Verificar si ya tiene folio asignado (idempotente)
    q.prepare("SELECT folio_documento FROM concesiones WHERE id = :id");
    q.bindValue(":id", static_cast<qlonglong>(concesionId));
    if (q.exec() && q.next()) {
        const QString existing = q.value(0).toString();
        if (!existing.isEmpty()) return existing;
    }

    // 2. Leer contador actual (valor = primer numero disponible)
    if (!q.exec("SELECT contador FROM folio_counters WHERE tipo = 'corte'") || !q.next()) {
        qWarning() << "FolioRepository: no se pudo leer contador corte";
        return QStringLiteral("(sin folio)");
    }
    const int numero = q.value(0).toInt();

    // 3. Incrementar contador para el proximo folio
    if (!q.exec("UPDATE folio_counters SET contador = contador + 1 WHERE tipo = 'corte'")) {
        qWarning() << "FolioRepository: error incrementando contador corte:" << q.lastError().text();
        return QStringLiteral("(sin folio)");
    }

    // 4. Formatear y almacenar
    const QString folio = formatearFolioCorte(numero, QDate::currentDate(), tipo);
    q.prepare("UPDATE concesiones SET folio_documento = :folio WHERE id = :id");
    q.bindValue(":folio", folio);
    q.bindValue(":id", static_cast<qlonglong>(concesionId));
    if (!q.exec()) {
        qWarning() << "FolioRepository: no se pudo guardar folio_documento:" << q.lastError().text();
    }
    return folio;
}

QString FolioRepository::generarFolioRI() {
    QSqlQuery q(m_db.database());

    // Leer contador actual
    if (!q.exec("SELECT contador FROM folio_counters WHERE tipo = 'reporte_interno'") || !q.next()) {
        qWarning() << "FolioRepository: no se pudo leer contador RI";
        return QStringLiteral("RI000-A");
    }
    const int contador = q.value(0).toInt();

    // Incrementar para el proximo
    if (!q.exec("UPDATE folio_counters SET contador = contador + 1 WHERE tipo = 'reporte_interno'")) {
        qWarning() << "FolioRepository: error incrementando contador RI:" << q.lastError().text();
    }

    return formatearFolioRI(contador);
}

} // namespace Calculadora
