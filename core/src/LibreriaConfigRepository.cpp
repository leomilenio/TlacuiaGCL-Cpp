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
    cfg.email             = q.value("email").toString();
    cfg.regimenFiscalCode = q.value("regimen_fiscal_code").toString();
    cfg.regimenFiscalDesc = q.value("regimen_fiscal_desc").toString();
    cfg.contactoNombre    = q.value("contacto_nombre").toString();
    cfg.dirCalle          = q.value("dir_calle").toString();
    cfg.dirNumExterior    = q.value("dir_num_exterior").toString();
    cfg.dirNumInterior    = q.value("dir_num_interior").toString();
    cfg.dirCodigoPostal   = q.value("dir_codigo_postal").toString();
    cfg.dirColonia        = q.value("dir_colonia").toString();
    cfg.dirMunicipio      = q.value("dir_municipio").toString();
    cfg.dirEstado         = q.value("dir_estado").toString();

    // Cargar hasta 4 telefonos; omitir filas donde el numero este vacio
    const QStringList tipoFields   = {"tel1_tipo",   "tel2_tipo",   "tel3_tipo",   "tel4_tipo"};
    const QStringList numeroFields = {"tel1_numero", "tel2_numero", "tel3_numero", "tel4_numero"};
    for (int i = 0; i < 4; ++i) {
        const QString numero = q.value(numeroFields[i]).toString();
        if (!numero.isEmpty()) {
            cfg.telefonos.append({ q.value(tipoFields[i]).toString(), numero });
        }
    }

    QVariant ll = q.value("logo_libreria");
    if (!ll.isNull()) cfg.logoLibreria = ll.toByteArray();
    cfg.logoLibreriaMime = q.value("logo_libreria_mime").toString();
    QVariant le = q.value("logo_empresa");
    if (!le.isNull()) cfg.logoEmpresa = le.toByteArray();
    cfg.logoEmpresaMime  = q.value("logo_empresa_mime").toString();
    return cfg;
}

bool LibreriaConfigRepository::save(const LibreriaConfig& cfg) {
    // Asegurar que no se guarden mas de 4 telefonos (limite del schema)
    const int nTel = qMin(cfg.telefonos.size(), 4);

    auto telTipo   = [&](int i) -> QString {
        return i < nTel ? cfg.telefonos[i].tipo   : QString{};
    };
    auto telNumero = [&](int i) -> QString {
        return i < nTel ? cfg.telefonos[i].numero : QString{};
    };

    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT OR REPLACE INTO app_config
            (id, libreria_nombre, empresa_nombre, rfc,
             email, regimen_fiscal_code, regimen_fiscal_desc,
             contacto_nombre,
             dir_calle, dir_num_exterior, dir_num_interior,
             dir_codigo_postal, dir_colonia, dir_municipio, dir_estado,
             tel1_tipo, tel1_numero, tel2_tipo, tel2_numero,
             tel3_tipo, tel3_numero, tel4_tipo, tel4_numero,
             logo_libreria, logo_libreria_mime,
             logo_empresa,  logo_empresa_mime)
        VALUES
            (1, :ln, :en, :rfc,
             :em, :rfc_c, :rfc_d,
             :cn,
             :dc, :dne, :dni,
             :dcp, :dcol, :dm, :de,
             :t1t, :t1n, :t2t, :t2n,
             :t3t, :t3n, :t4t, :t4n,
             :ll, :llm, :le, :lem)
    )");
    q.bindValue(":ln",    cfg.libreriaNombre);
    q.bindValue(":en",    cfg.empresaNombre);
    q.bindValue(":rfc",   cfg.rfc);
    q.bindValue(":em",    cfg.email);
    q.bindValue(":rfc_c", cfg.regimenFiscalCode);
    q.bindValue(":rfc_d", cfg.regimenFiscalDesc);
    q.bindValue(":cn",    cfg.contactoNombre);
    q.bindValue(":dc",    cfg.dirCalle);
    q.bindValue(":dne",   cfg.dirNumExterior);
    q.bindValue(":dni",   cfg.dirNumInterior);
    q.bindValue(":dcp",   cfg.dirCodigoPostal);
    q.bindValue(":dcol",  cfg.dirColonia);
    q.bindValue(":dm",    cfg.dirMunicipio);
    q.bindValue(":de",    cfg.dirEstado);
    q.bindValue(":t1t",   telTipo(0));
    q.bindValue(":t1n",   telNumero(0));
    q.bindValue(":t2t",   telTipo(1));
    q.bindValue(":t2n",   telNumero(1));
    q.bindValue(":t3t",   telTipo(2));
    q.bindValue(":t3n",   telNumero(2));
    q.bindValue(":t4t",   telTipo(3));
    q.bindValue(":t4n",   telNumero(3));

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
