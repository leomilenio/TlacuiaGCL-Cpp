#pragma once
#include <QDialog>
#include "core/ConcesionRepository.h"

namespace App {

class AlertDialog : public QDialog {
    Q_OBJECT
public:
    explicit AlertDialog(const QList<Calculadora::ConcesionRecord>& concesiones,
                         QWidget* parent = nullptr);
};

} // namespace App
