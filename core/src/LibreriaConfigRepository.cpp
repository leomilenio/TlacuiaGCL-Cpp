#include "core/LibreriaConfigRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace Calculadora {

LibreriaConfigRepository::LibreriaConfigRepository(DatabaseManager& dbManager)
    : m_db(dbManager)
{
}

LibreriaConfig LibreriaConfigRepository::load() const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM app_config WHERE id = 1");
    LibreriaConfig cfg;
    if (!q.exec() || !q.next()) {
        qDebug() << "LibreriaConfigRepo::load: sin fila en app_config (se usaran valores vacios)";
        return cfg;
    }
    cfg.libreriaNombre    = q.value("libreria_nombre").toString();
    cfg.empresaNombre     = q.value("empresa_nombre").toString();
    cfg.rfc               = q.value("rfc").toString();
    cfg.tel1              = q.value("tel1").toString();
    cfg.tel2              = q.value("tel2").toString();
    QVariant ll           = q.value("logo_libreria");
    if (!ll.isNull()) cfg.logoLibreria = ll.toByteArray();
    cfg.logoLibreriaMime  = q.value("logo_libreria_mime").toString();
    QVariant le           = q.value("logo_empresa");
    if (!le.isNull()) cfg.logoEmpresa = le.toByteArray();
    cfg.logoEmpresaMime   = q.value("logo_empresa_mime").toString();
    return cfg;
}

bool LibreriaConfigRepository::save(const LibreriaConfig& cfg) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT OR REPLACE INTO app_config
            (id, libreria_nombre, empresa_nombre, rfc, tel1, tel2,
             logo_libreria, logo_libreria_mime,
             logo_empresa,  logo_empresa_mime)
        VALUES
            (1, :ln, :en, :rfc, :t1, :t2,
             :ll, :llm,
             :le, :lem)
    )");
    q.bindValue(":ln",  cfg.libreriaNombre);
    q.bindValue(":en",  cfg.empresaNombre);
    q.bindValue(":rfc", cfg.rfc);
    q.bindValue(":t1",  cfg.tel1);
    q.bindValue(":t2",  cfg.tel2);

    auto bindBlob = [&](const QString& key, const QByteArray& data) {
        if (data.isEmpty())
            q.bindValue(key, QVariant(QMetaType(QMetaType::QByteArray)));
        else
            q.bindValue(key, data);
    };
    bindBlob(":ll", cfg.logoLibreria);
    q.bindValue(":llm", cfg.logoLibreriaMime.isEmpty()
                        ? QVariant(QMetaType(QMetaType::QString))
                        : QVariant(cfg.logoLibreriaMime));
    bindBlob(":le", cfg.logoEmpresa);
    q.bindValue(":lem", cfg.logoEmpresaMime.isEmpty()
                        ? QVariant(QMetaType(QMetaType::QString))
                        : QVariant(cfg.logoEmpresaMime));

    if (!q.exec()) {
        qCritical() << "LibreriaConfigRepo::save:" << q.lastError().text();
        return false;
    }
    return true;
}

} // namespace Calculadora
