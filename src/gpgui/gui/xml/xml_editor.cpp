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

#include "xml_editor.h"
#include "xml_edit.h"
#include "xml_string_edit.h"
#include "xml_bool_edit.h"
#include "xml_attribute.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFile>
#include <QXmlStreamReader>
#include <QDomDocument>
#include <QPushButton>

QList<XmlAttribute> XmlEditor::schema_attributes;
QHash<QString, XmlAttribute> XmlEditor::schema_attributes_by_name;

void XmlEditor::load_schema() {
    static bool loaded = false;
    if (loaded) {
        return;
    } else {
        loaded = true;
    }

    QFile file(":/shortcuts_xml_schema.xml");
    const bool open_success = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!open_success) {
        printf("Failed to open xml file\n");
        return;
    }
    QDomDocument doc;
    doc.setContent(&file);
    file.close();

    const QDomNodeList attributes = doc.elementsByTagName("xs:attribute");
    for (int i = 0; i < attributes.size(); i++) {
        const QDomNode node = attributes.at(i);
        const XmlAttribute attribute(node);

        schema_attributes.append(attribute);
        schema_attributes_by_name.insert(attribute.name(), attribute);
    }
}

XmlEditor::XmlEditor(const QString &path_arg)
: QDialog()
{   
    path = path_arg;

    load_schema();

    setAttribute(Qt::WA_DeleteOnClose);
    resize(300, 600);

    const QString title_label_text = tr("Editing xml file:") + path;
    auto title_label = new QLabel(title_label_text);

    auto button_box = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    connect(
        button_box, &QDialogButtonBox::accepted,
        this, &QDialog::accept);
    connect(
        button_box, &QDialogButtonBox::rejected,
        this, &QDialog::reject);
    
    QFile file(path);
    const bool open_success = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!open_success) {
        printf("Failed to open xml file\n");
        return;
    }

    QDomDocument doc;
    doc.setContent(&file);

    file.close();

    auto edits_layout = new QGridLayout();

    for (auto attribute : schema_attributes) {
        if (attribute.hidden()) {
            continue;
        }

        auto make_edit =
        [attribute]() -> XmlEdit * {
            switch (attribute.type()) {
                case XmlAttributeType_String:
                return new XmlStringEdit(attribute);

                case XmlAttributeType_Boolean:
                return new XmlBoolEdit(attribute);

                case XmlAttributeType_UnsignedByte:
                return new XmlStringEdit(attribute);

                case XmlAttributeType_None:
                return nullptr;
            }

            return nullptr;
        };
        XmlEdit *edit = make_edit();
        edit->add_to_layout(edits_layout);
        edit->load(doc);

        edits.append(edit);
    }

    const auto top_layout = new QVBoxLayout();
    setLayout(top_layout);
    top_layout->addWidget(title_label);
    top_layout->addLayout(edits_layout);
    top_layout->addWidget(button_box);

    connect(
        button_box->button(QDialogButtonBox::Ok), &QPushButton::clicked,
        this, &XmlEditor::on_ok);
}

void XmlEditor::on_ok() {
    // Read doc into memory
    QFile read_file(path);
    const bool opened_read_file = read_file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!opened_read_file) {
        printf("Failed to open xml file for reading\n");
        return;
    }
    QDomDocument doc;
    doc.setContent(&read_file);
    read_file.close();

    // Apply changes to doc in memory
    for (auto edit : edits) {
        if (edit->changed()) {
            edit->apply(&doc);
        }
    }

    // Save updated doc to file
    QFile write_file(path);
    const bool opened_write_file = write_file.open(QIODevice::QIODevice::WriteOnly | QIODevice::Truncate);
    if (!opened_write_file) {
        printf("Failed to open xml file for writing\n");
        return;
    }
    const QByteArray doc_bytes = doc.toByteArray(4);
    const char *doc_cstr = doc_bytes.constData();
    write_file.write(doc_cstr);
    write_file.close();
}
