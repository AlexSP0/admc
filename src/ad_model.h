
#ifndef AD_MODEL_H
#define AD_MODEL_H

#include <QStandardItemModel>

void load_and_add_row(QStandardItem *parent, QString &dn);

class AdModel: public QStandardItemModel {
Q_OBJECT

public:
    enum Column {
        Name,
        Category,
        Description,
        DN,
        COUNT,
    };

    explicit AdModel();

    enum Roles {
        AdvancedViewOnly = Qt::UserRole + 1,
        CanFetch = Qt::UserRole + 2,
        IsContainer = Qt::UserRole + 3,
    };

    bool canFetchMore(const QModelIndex &parent) const;
    void fetchMore(const QModelIndex &parent);
    bool hasChildren(const QModelIndex &parent) const override;

public slots:
    void on_entry_changed(const QString &dn); 
    void on_entry_deleted(const QString &dn); 

private:

};

#endif /* AD_MODEL_H */