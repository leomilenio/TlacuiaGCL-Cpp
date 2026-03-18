#pragma once
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <cstdint>

namespace Calculadora {

struct EmisorRecord {
    int64_t id             = 0;
    QString nombreEmisor;       // nombre del distribuidor / editorial
    QString nombreVendedor;     // nombre del representante de ventas
    QString telefono;
    QString email;
    QString notas;
    QString createdAt;
};

class DatabaseManager;

class EmisorRepository {
public:
    explicit EmisorRepository(DatabaseManager& dbManager);

    [[nodiscard]] QList<EmisorRecord> findAll()           const;
    [[nodiscard]] EmisorRecord        findById(int64_t id) const;
    [[nodiscard]] int64_t             save(const EmisorRecord& record);
    [[nodiscard]] bool                update(const EmisorRecord& record);
    [[nodiscard]] bool                remove(int64_t id);

private:
    [[nodiscard]] EmisorRecord mapRow(const QSqlQuery& q) const;
    DatabaseManager& m_db;
};

} // namespace Calculadora
