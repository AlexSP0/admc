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

#ifndef ADMC_TEST_STRING_EDIT_H
#define ADMC_TEST_STRING_EDIT_H

#include "admc_test.h"

class StringEdit;
class QLineEdit;

class ADMCTestStringEdit : public ADMCTest {
Q_OBJECT

private slots:
    void init() override;

    void test_emit_edited_signal();
    void load();
    void apply_unmodified();
    void apply_modified();

private:
    StringEdit *edit;
    QLineEdit *line_edit;
    QString dn;
};

#endif /* ADMC_TEST_STRING_EDIT_H */
