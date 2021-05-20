/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "edit_query_item_widget.h"

#include "ad_filter.h"
#include "console_types/console_query.h"
#include "filter_widget/filter_widget.h"
#include "filter_widget/search_base_widget.h"

#include <QLineEdit>
#include <QTextEdit>
#include <QFormLayout>
#include <QDialog>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>

EditQueryItemWidget::EditQueryItemWidget()
: QWidget()
{
    setMinimumWidth(400);

    search_base_widget = new SearchBaseWidget();

    filter_widget = new FilterWidget(filter_classes);

    name_edit = new QLineEdit();
    
    description_edit = new QLineEdit();

    filter_edit = new QTextEdit();
    filter_edit->setReadOnly(true);

    auto edit_filter_dialog = new QDialog(this);
    edit_filter_dialog->setWindowTitle("Edit filter");

    auto dialog_buttonbox = new QDialogButtonBox();
    dialog_buttonbox->addButton(QDialogButtonBox::Ok);

    auto dialog_layout = new QVBoxLayout();
    edit_filter_dialog->setLayout(dialog_layout);
    dialog_layout->addWidget(filter_widget);
    dialog_layout->addWidget(dialog_buttonbox);

    auto edit_filter_button = new QPushButton(tr("Edit filter"));

    auto form_layout = new QFormLayout();
    form_layout->addRow(tr("Name:"), name_edit);
    form_layout->addRow(tr("Description:"), description_edit);
    form_layout->addRow(tr("Search in:"), search_base_widget);
    form_layout->addRow(new QLabel(tr("Filter:")));
    form_layout->addRow(filter_edit);
    form_layout->addRow(edit_filter_button);

    const auto layout = new QVBoxLayout();
    setLayout(layout);
    layout->addLayout(form_layout);
    layout->addWidget(edit_filter_button);

    connect(
        dialog_buttonbox, &QDialogButtonBox::accepted,
        edit_filter_dialog, &QDialog::accept);

    connect(
        edit_filter_button, &QPushButton::clicked,
        edit_filter_dialog, &QDialog::open);
    connect(
        edit_filter_dialog, &QDialog::accepted,
        this, &EditQueryItemWidget::on_edit_filter_dialog_accepted);
    on_edit_filter_dialog_accepted();
}

void EditQueryItemWidget::load(const QModelIndex &index) {
    QByteArray filter_state = index.data(QueryItemRole_FilterState).toByteArray();
    QDataStream filter_state_stream(filter_state);
    filter_state_stream >> search_base_widget;
    filter_state_stream >> filter_widget;

    const QString name = index.data(Qt::DisplayRole).toString();
    name_edit->setText(name);

    const QString description = index.data(QueryItemRole_Description).toString();
    description_edit->setText(description);
}

void EditQueryItemWidget::get_state(QString &name, QString &description, QString &filter, QString &search_base, QByteArray &filter_state) const {
    name = name_edit->text();
    description = description_edit->text();
    filter = filter_widget->get_filter();
    search_base = search_base_widget->get_search_base();

    filter_state =
    [&]() {
        QByteArray out;

        QDataStream filter_state_stream(&out, QIODevice::WriteOnly);
        filter_state_stream << search_base_widget;
        filter_state_stream << filter_widget;

        return out;
    }();
}

void EditQueryItemWidget::on_edit_filter_dialog_accepted() {
    const QString filter = filter_widget->get_filter();
    filter_edit->setPlainText(filter);
}