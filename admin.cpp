#include <QApplication>
#include <QTableView>
#include <QStandardItemModel>
#include <QHBoxLayout>   // Добавлено: для кнопок в ряд
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

public:
    AdminPanel() {
        setWindowTitle("Панель Модератора (Network Mode)");
        resize(800, 600);

        manager = new QNetworkAccessManager(this);
        model = new QStandardItemModel(this);
        model->setHorizontalHeaderLabels({"ID", "Date", "Name", "Type", "Prompt", "Status"});

        view = new QTableView();
        view->setModel(model);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        auto *btnApprove = new QPushButton("Одобрить ✅");
        auto *btnReject = new QPushButton("Отклонить ❌");
        auto *btnRefresh = new QPushButton("Обновить 🔄");

        auto *layout = new QVBoxLayout(this);
        auto *btnLayout = new QHBoxLayout(); // Теперь QHBoxLayout подключен
        btnLayout->addWidget(btnApprove);
        btnLayout->addWidget(btnReject);
        btnLayout->addWidget(btnRefresh);

        layout->addWidget(view);
        layout->addLayout(btnLayout);

        connect(btnRefresh, &QPushButton::clicked, this, &AdminPanel::loadData);
        connect(btnApprove, &QPushButton::clicked, [this](){ updateStatus("approved"); });
        connect(btnReject, &QPushButton::clicked, [this](){ updateStatus("rejected"); });

        loadData();
    }

    void loadData() {
        // Убедитесь, что сервер запущен на этом порту
        QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("http://127.0.0.1:8080/get_messages")));

        connect(reply, &QNetworkReply::finished, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                model->removeRows(0, model->rowCount());
                QByteArray responseData = reply->readAll();
                QJsonArray array = QJsonDocument::fromJson(responseData).array();

                for (const QJsonValue &v : array) {
                    QJsonObject obj = v.toObject();

                    // ВАЖНО: Ключи должны совпадать с item["key"] на сервере!
                    QList<QStandardItem*> row = {
                        new QStandardItem(obj["id"].toVariant().toString()),
                        new QStandardItem(obj["created_at"].toString()),
                        new QStandardItem(obj["fio"].toString()),
                        new QStandardItem(obj["content_type"].toString()),
                        new QStandardItem(obj["prompt_text"].toString()),
                        new QStandardItem(obj["status"].toString())
                    };
                    model->appendRow(row);
                }
            }
            reply->deleteLater();
        });
    }

    void updateStatus(QString status) {
        auto selected = view->selectionModel()->selectedRows();
        for (const auto &index : selected) {
            QString id = model->item(index.row(), 0)->text();

            QUrlQuery params;
            params.addQueryItem("id", id);
            params.addQueryItem("status", status);

            QNetworkRequest request(QUrl("http://127.0.0.1:8080/update_status"));
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

            QNetworkReply *reply = manager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
            connect(reply, &QNetworkReply::finished, [this, reply]() {
                loadData(); // Перезагружаем список после обновления
                reply->deleteLater();
            });
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    AdminPanel w;
    w.show();
    return a.exec();
}

#include "admin.moc" // Имя совпадает с admin.cpp