#include <QApplication>
#include <QTableView>
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QHeaderView>

class AdminPanel : public QWidget {
    Q_OBJECT
    QStandardItemModel *model;
    QTableView *view;
    QNetworkAccessManager *manager;
    // Выносим кнопки в поля класса для доступа из других методов
    QPushButton *btnApprove;
    QPushButton *btnReject;

public:
    AdminPanel() {
        setWindowTitle("Панель Модератора (NeuroArt)");
        resize(900, 600);

        manager = new QNetworkAccessManager(this);
        model = new QStandardItemModel(this);
        model->setHorizontalHeaderLabels({"ID", "Дата", "ФИО", "ТИП", "Запрос", "Статус"});

        view = new QTableView();
        view->setModel(model);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        btnApprove = new QPushButton("Одобрить ✅");
        btnReject = new QPushButton("Отклонить ❌");
        auto *btnRefresh = new QPushButton("Обновить 🔄");

        // По умолчанию кнопки выключены, пока ничего не выбрано
        btnApprove->setEnabled(false);
        btnReject->setEnabled(false);

        auto *layout = new QVBoxLayout(this);
        auto *btnLayout = new QHBoxLayout();
        btnLayout->addWidget(btnApprove);
        btnLayout->addWidget(btnReject);
        btnLayout->addWidget(btnRefresh);

        layout->addWidget(view);
        layout->addLayout(btnLayout);

        // Логика блокировки кнопок при выборе строки (ТЗ 3.3)
        connect(view->selectionModel(), &QItemSelectionModel::currentRowChanged, [this](const QModelIndex &current) {
            if (!current.isValid()) return;
            // Проверяем текст в колонке "Статус" (индекс 5)
            QString status = model->item(current.row(), 5)->data(Qt::UserRole).toString();
            bool isNew = (status == "pending");
            btnApprove->setEnabled(isNew);
            btnReject->setEnabled(isNew);
        });

        connect(btnRefresh, &QPushButton::clicked, this, &AdminPanel::loadData);
        connect(btnApprove, &QPushButton::clicked, [this]() { updateStatus("approved"); });
        connect(btnReject, &QPushButton::clicked, [this]() { updateStatus("rejected"); });

        loadData();
    }

    void loadData() {
        QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("http://127.0.0.1:8080/get_messages")));

        connect(reply, &QNetworkReply::finished, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                model->removeRows(0, model->rowCount());
                QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();

                for (const QJsonValue &v : array) {
                    QJsonObject obj = v.toObject();
                    QString rawStatus = obj["status"].toString();

                    // Красивое отображение для модератора
                    QString displayStatus = (rawStatus == "pending") ? "⏳ Жаңа (Новый)" :
                                            (rawStatus == "approved") ? "✅ Мақұлданды (Одобрено)" : "❌ Қабылданбады (Отклонено)";

                    QList<QStandardItem *> row = {
                        new QStandardItem(obj["id"].toVariant().toString()),
                        new QStandardItem(obj["created_at"].toString()),
                        new QStandardItem(obj["fio"].toString()),
                        new QStandardItem(obj["content_type"].toString()),
                        new QStandardItem(obj["prompt_text"].toString()),
                        new QStandardItem(displayStatus)
                    };

                    // Сохраняем "чистый" статус в скрытые данные ячейки для логики кнопок
                    row.last()->setData(rawStatus, Qt::UserRole);
                    model->appendRow(row);
                }
            }
            reply->deleteLater();
        });
    }

    void updateStatus(QString status) {
        auto selected = view->selectionModel()->selectedRows();
        if (selected.isEmpty()) return;

        QString id = model->item(selected.first().row(), 0)->text();
        QUrlQuery params;
        params.addQueryItem("id", id);
        params.addQueryItem("status", status);

        QNetworkRequest request(QUrl("http://127.0.0.1:8080/update_status"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QNetworkReply *reply = manager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, [this, reply]() {
            loadData(); // Мгновенное обновление таблицы
            reply->deleteLater();
        });
    }
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    AdminPanel w;
    w.show();
    return a.exec();
}

#include "admin.moc" // Имя совпадает с admin.cpp