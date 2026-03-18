#include "app/AlertDialog.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace App {

AlertDialog::AlertDialog(const QList<Calculadora::ConcesionRecord>& concesiones,
                         QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Alertas de Concesiones");
    setMinimumWidth(500);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* titleLabel = new QLabel(
        QString("<b>%1 concesion(es) proximas a vencer o vencidas</b>")
            .arg(concesiones.size()));
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto* list = new QListWidget();
    list->setAlternatingRowColors(true);
    list->setFocusPolicy(Qt::NoFocus);

    for (const auto& c : concesiones) {
        int dias = c.diasRestantes();
        QString statusText;
        if (dias < 0)
            statusText = QString("VENCIDA hace %1 dias").arg(-dias);
        else if (dias == 0)
            statusText = "Vence HOY";
        else
            statusText = QString("Vence en %1 dias").arg(dias);

        QString emisor = c.emisorNombre.isEmpty() ? "(Sin emisor)" : c.emisorNombre;
        QString folio  = c.folio.isEmpty()        ? "(Sin folio)"  : c.folio;
        list->addItem(QString("%1  \u2014  Folio: %2  \u2014  %3")
                          .arg(emisor, folio, statusText));
    }
    layout->addWidget(list);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* btnOk = new QPushButton("Entendido");
    btnOk->setDefault(true);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(btnOk);
    layout->addLayout(btnRow);
}

} // namespace App
