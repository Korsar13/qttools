// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "line_propertysheet.h"
#include "formwindow.h"

// sdk
#include <QtDesigner/qextensionmanager.h>

#include <QtWidgets/qlayout.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

LinePropertySheet::LinePropertySheet(Line *object, QObject *parent)
    : QDesignerPropertySheet(object, parent)
{
    clearFakeProperties();
}

LinePropertySheet::~LinePropertySheet() = default;

bool LinePropertySheet::isVisible(int index) const
{
    const QString name = propertyName(index);

    if (name == QStringLiteral("frameShape"))
        return false;
    return QDesignerPropertySheet::isVisible(index);
}

void LinePropertySheet::setProperty(int index, const QVariant &value)
{
    QDesignerPropertySheet::setProperty(index, value);
}

QString LinePropertySheet::propertyGroup(int index) const
{
    return QDesignerPropertySheet::propertyGroup(index);
}

QT_END_NAMESPACE
