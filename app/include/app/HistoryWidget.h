#pragma once
#include <QWidget>

class QTableView;
class QPushButton;

namespace Calculadora { class ProductoRepository; }

namespace App {
class HistoryTableModel;

class HistoryWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistoryWidget(Calculadora::ProductoRepository& repo,
                            QWidget* parent = nullptr);

public slots:
    void refresh();

private slots:
    void onDeleteSelected();

private:
    void setupUi();
    void setupConnections();

    QTableView*        m_tableView = nullptr;
    QPushButton*       m_deleteBtn = nullptr;
    HistoryTableModel* m_model     = nullptr;

    Calculadora::ProductoRepository& m_repo;
};

} // namespace App
