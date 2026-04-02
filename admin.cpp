#include <QApplication>
#include <QHeaderView>
#include <QSqlTableModel>
#include <QTableView>
#include <QMessageBox>
#include <QSqlError>
// Добавляем недостающие инклуды для интерфейса
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. Подключаемся к БД
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("/Users/kanapiya/Documents/cpp_server/cmake-build-debug/projectDB.db");

    if (!db.open()) {
        QMessageBox::critical(nullptr, "Ошибка БД", db.lastError().text());
        return -1;
    }

    // 2. Настраиваем модель
    QSqlTableModel model;
    model.setTable("generation_requests");
    // OnManualSubmit лучше подходит для кнопок, чтобы изменения сохранялись только после updateStatus
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);

    if (!model.select()) {
        QMessageBox::warning(nullptr, "Ошибка данных", "Не удалось прочитать таблицу: " + model.lastError().text());
    }

    // 3. Создаем основное окно и лейауты
    QWidget window;
    window.setWindowTitle("Панель Модератора");
    QVBoxLayout *mainLayout = new QVBoxLayout(&window);

    // 4. Настраиваем таблицу
    QTableView *view = new QTableView(); // Создаем через new, так как кладем в Layout
    view->setModel(&model);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // Все колонки по ширине
    view->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделять всю строку целиком

    mainLayout->addWidget(view);

    // 5. Создаем панель кнопок
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *btnApprove = new QPushButton("Одобрить ✅");
    QPushButton *btnReject = new QPushButton("Отклонить ❌");

    buttonLayout->addWidget(btnApprove);
    buttonLayout->addWidget(btnReject);
    mainLayout->addLayout(buttonLayout);

    // Лямбда-функция для смены статуса
    auto updateStatus = [&](QString newStatus) {
        // Получаем индексы выбранных строк
        QModelIndexList selected = view->selectionModel()->selectedRows();
        if (selected.isEmpty()) {
            QMessageBox::information(&window, "Внимание", "Выберите строку в таблице!");
            return;
        }

        for (const QModelIndex &index : selected) {
            // 4 — это индекс колонки 'status' (id=0, user_id=1, type=2, prompt=3, status=4)
            model.setData(model.index(index.row(), 4), newStatus);
        }

        if (model.submitAll()) {
            model.select(); // Перечитываем данные для обновления вида
        } else {
            QMessageBox::critical(&window, "Ошибка", "Не удалось сохранить: " + model.lastError().text());
        }
    };

    // Коннектим кнопки
    QObject::connect(btnApprove, &QPushButton::clicked, [&](){ updateStatus("approved"); });
    QObject::connect(btnReject, &QPushButton::clicked, [&](){ updateStatus("rejected"); });

    window.resize(1000, 700);
    window.show();

    return a.exec();
}
