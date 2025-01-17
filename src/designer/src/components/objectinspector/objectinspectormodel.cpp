// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "objectinspectormodel_p.h"

#include <qlayout_widget_p.h>
#include <layout_p.h>
#include <qdesigner_propertycommand_p.h>
#include <qdesigner_utils_p.h>
#include <iconloader_p.h>

#include <QtDesigner/abstractformeditor.h>
#include <QtDesigner/abstractformwindow.h>
#include <QtDesigner/abstractwidgetdatabase.h>
#include <QtDesigner/container.h>
#include <QtDesigner/abstractmetadatabase.h>
#include <QtDesigner/qextensionmanager.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qbuttongroup.h>

#include <QtGui/qaction.h>

#include <QtCore/qset.h>
#include <QtCore/qdebug.h>
#include <QtCore/qcoreapplication.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace {
    enum { DataRole = 1000 };
}

static inline QObject *objectOfItem(const QStandardItem *item) {
    return qvariant_cast<QObject *>(item->data(DataRole));
}

static bool sameIcon(const QIcon &i1, const QIcon &i2)
{
    if (i1.isNull() &&  i2.isNull())
             return true;
    if (i1.isNull() !=  i2.isNull())
        return false;
    return i1.cacheKey() == i2.cacheKey();
}

static inline bool isNameColumnEditable(const QObject *)
{
    return true;
}

static qdesigner_internal::ObjectData::StandardItemList createModelRow(const QObject *o)
{
    qdesigner_internal::ObjectData::StandardItemList rc;
    const Qt::ItemFlags baseFlags = Qt::ItemIsSelectable|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled;
    for (int i = 0; i < qdesigner_internal::ObjectInspectorModel::NumColumns; i++) {
        QStandardItem *item = new QStandardItem;
        Qt::ItemFlags flags = baseFlags;
        if (i == qdesigner_internal::ObjectInspectorModel::ObjectNameColumn && isNameColumnEditable(o))
            flags |= Qt::ItemIsEditable;
        item->setFlags(flags);
        rc += item;
    }
    return rc;
}

static inline bool isQLayoutWidget(const QObject *o)
{
    return o->metaObject() == &QLayoutWidget::staticMetaObject;
}

namespace qdesigner_internal {

    // context kept while building a model, just there to reduce string allocations
    struct ModelRecursionContext {
        explicit ModelRecursionContext(QDesignerFormEditorInterface *core, const QString &sepName);

        const QString designerPrefix;
        const QString separator;

        QDesignerFormEditorInterface *core;
        const QDesignerWidgetDataBaseInterface *db;
        const QDesignerMetaDataBaseInterface *mdb;
    };

    ModelRecursionContext::ModelRecursionContext(QDesignerFormEditorInterface *c, const QString &sepName) :
        designerPrefix(QStringLiteral("QDesigner")),
        separator(sepName),
        core(c),
        db(c->widgetDataBase()),
        mdb(c->metaDataBase())
    {
    }

    // ------------  ObjectData/ ObjectModel:
    // Whenever the selection changes, ObjectInspector::setFormWindow is
    // called. To avoid rebuilding the tree every time (loosing expanded state)
    // a model is first built from the object tree by recursion.
    // As a tree is difficult to represent, a flat list of entries (ObjectData)
    // containing object and parent object is used.
    // ObjectData has an overloaded operator== that compares the object pointers.
    // Structural changes which cause a rebuild can be detected by
    // comparing the lists of ObjectData. If it is the same, only the item data (class name [changed by promotion],
    // object name and icon) are checked and the existing items are updated.

    ObjectData::ObjectData() = default;

    ObjectData::ObjectData(QObject *parent, QObject *object, const ModelRecursionContext &ctx) :
       m_parent(parent),
       m_object(object),
       m_className(QLatin1String(object->metaObject()->className())),
       m_objectName(object->objectName())
    {

        // 1) set entry
        if (object->isWidgetType()) {
            initWidget(static_cast<QWidget*>(object), ctx);
        } else {
            initObject(ctx);
        }
        if (m_className.startsWith(ctx.designerPrefix))
            m_className.remove(1, ctx.designerPrefix.size() - 1);
    }

    void ObjectData::initObject(const ModelRecursionContext &ctx)
    {
        // Check objects: Action?
        if (const QAction *act = qobject_cast<const QAction*>(m_object)) {
            if (act->isSeparator()) {  // separator is reserved
                m_objectName = ctx.separator;
                m_type = SeparatorAction;
            } else {
                m_type = Action;
            }
            m_classIcon = act->icon();
        } else {
            m_type = Object;
        }
    }

    void ObjectData::initWidget(QWidget *w, const ModelRecursionContext &ctx)
    {
        // Check for extension container, QLayoutwidget, or normal container
        bool isContainer = false;
        if (const QDesignerWidgetDataBaseItemInterface *widgetItem = ctx.db->item(ctx.db->indexOfObject(w, true))) {
            m_classIcon = widgetItem->icon();
            m_className = widgetItem->name();
            isContainer = widgetItem->isContainer();
        }

        // We might encounter temporary states with no layouts when re-layouting.
        // Just default to Widget handling for the moment.
        if (isQLayoutWidget(w)) {
            if (const QLayout *layout = w->layout()) {
                m_type = LayoutWidget;
                m_managedLayoutType = LayoutInfo::layoutType(ctx.core, layout);
                m_className = QLatin1String(layout->metaObject()->className());
                m_objectName = layout->objectName();
            }
            return;
        }

        if (qt_extension<QDesignerContainerExtension*>(ctx.core->extensionManager(), w)) {
            m_type = ExtensionContainer;
            return;
        }
        if (isContainer) {
            m_type = LayoutableContainer;
            m_managedLayoutType = LayoutInfo::managedLayoutType(ctx.core, w);
            return;
        }
        m_type = ChildWidget;
    }

    bool ObjectData::equals(const ObjectData & me) const
    {
        return m_parent == me.m_parent && m_object == me.m_object;
    }

    unsigned ObjectData::compare(const ObjectData & rhs) const
    {
        unsigned rc = 0;
        if (m_className != rhs.m_className)
            rc |= ClassNameChanged;
        if (m_objectName != rhs.m_objectName)
            rc |= ObjectNameChanged;
        if (!sameIcon(m_classIcon, rhs.m_classIcon))
            rc |= ClassIconChanged;
        if (m_type != rhs.m_type)
            rc |= TypeChanged;
        if (m_managedLayoutType != rhs.m_managedLayoutType)
            rc |= LayoutTypeChanged;
        return rc;
    }

    void ObjectData::setItemsDisplayData(const StandardItemList &row, const ObjectInspectorIcons &icons, unsigned mask) const
    {
        if (mask & ObjectNameChanged)
            row[ObjectInspectorModel::ObjectNameColumn]->setText(m_objectName);
        if (mask & ClassNameChanged) {
            row[ObjectInspectorModel::ClassNameColumn]->setText(m_className);
            row[ObjectInspectorModel::ClassNameColumn]->setToolTip(m_className);
        }
        // Set a layout icon only for containers. Note that QLayoutWidget don't have
        // real class icons
        if (mask & (ClassIconChanged|TypeChanged|LayoutTypeChanged)) {
            switch (m_type) {
            case LayoutWidget:
                row[ObjectInspectorModel::ObjectNameColumn]->setIcon(icons.layoutIcons[m_managedLayoutType]);
                row[ObjectInspectorModel::ClassNameColumn]->setIcon(icons.layoutIcons[m_managedLayoutType]);
                break;
            case LayoutableContainer:
                row[ObjectInspectorModel::ObjectNameColumn]->setIcon(icons.layoutIcons[m_managedLayoutType]);
                row[ObjectInspectorModel::ClassNameColumn]->setIcon(m_classIcon);
                break;
            default:
                row[ObjectInspectorModel::ObjectNameColumn]->setIcon(QIcon());
                row[ObjectInspectorModel::ClassNameColumn]->setIcon(m_classIcon);
                break;
            }
        }
    }

    void ObjectData::setItems(const StandardItemList &row, const ObjectInspectorIcons &icons) const
    {
        const QVariant object = QVariant::fromValue(m_object);
        row[ObjectInspectorModel::ObjectNameColumn]->setData(object, DataRole);
        row[ObjectInspectorModel::ClassNameColumn]->setData(object, DataRole);
        setItemsDisplayData(row, icons, ClassNameChanged|ObjectNameChanged|ClassIconChanged|TypeChanged|LayoutTypeChanged);
    }

    // Recursive routine that creates the model by traversing the form window object tree.
    void createModelRecursion(const QDesignerFormWindowInterface *fwi,
                              QObject *parent,
                              QObject *object,
                              ObjectModel &model,
                              const ModelRecursionContext &ctx)
    {
        using ButtonGroupList = QList<QButtonGroup *>;
        // 1) Create entry
        const ObjectData entry(parent, object, ctx);
        model.push_back(entry);

        // 2) recurse over widget children via container extension or children list
        const QDesignerContainerExtension *containerExtension = nullptr;
        if (entry.type() == ObjectData::ExtensionContainer) {
            containerExtension = qt_extension<QDesignerContainerExtension*>(fwi->core()->extensionManager(), object);
            Q_ASSERT(containerExtension);
            const int count = containerExtension->count();
            for (int i=0; i < count; ++i) {
                QObject *page = containerExtension->widget(i);
                Q_ASSERT(page != nullptr);
                createModelRecursion(fwi, object, page, model, ctx);
            }
        }

        if (!object->children().isEmpty()) {
            ButtonGroupList buttonGroups;
            for (QObject *childObject : object->children()) {
                // Managed child widgets unless we had a container extension
                if (childObject->isWidgetType()) {
                    if (!containerExtension) {
                        QWidget *widget = qobject_cast<QWidget*>(childObject);
                        if (fwi->isManaged(widget))
                            createModelRecursion(fwi, object, widget, model, ctx);
                    }
                } else {
                    if (ctx.mdb->item(childObject)) {
                        if (auto bg = qobject_cast<QButtonGroup*>(childObject))
                            buttonGroups.push_back(bg);
                    } // Has MetaDataBase entry
                }
            }
            // Add button groups
            if (!buttonGroups.isEmpty()) {
                for (QButtonGroup *group : qAsConst(buttonGroups))
                    createModelRecursion(fwi, object, group, model, ctx);
            }
        } // has children
        if (object->isWidgetType()) {
            // Add actions
            const auto actions = static_cast<QWidget*>(object)->actions();
            for (QAction *action : actions) {
                if (ctx.mdb->item(action)) {
                    QObject *childObject = action;
                    if (auto menu = action->menu())
                        childObject = menu;
                    createModelRecursion(fwi, object, childObject, model, ctx);
                }
            }
        }
    }

    // ------------ ObjectInspectorModel
    ObjectInspectorModel::ObjectInspectorModel(QObject *parent) :
       QStandardItemModel(0, NumColumns, parent)
    {
        QStringList headers;
        headers += QCoreApplication::translate("ObjectInspectorModel", "Object");
        headers += QCoreApplication::translate("ObjectInspectorModel", "Class");
        Q_ASSERT(headers.size() == NumColumns);
        setColumnCount(NumColumns);
        setHorizontalHeaderLabels(headers);
        // Icons
        m_icons.layoutIcons[LayoutInfo::NoLayout] = createIconSet(QStringLiteral("editbreaklayout.png"));
        m_icons.layoutIcons[LayoutInfo::HSplitter] = createIconSet(QStringLiteral("edithlayoutsplit.png"));
        m_icons.layoutIcons[LayoutInfo::VSplitter] = createIconSet(QStringLiteral("editvlayoutsplit.png"));
        m_icons.layoutIcons[LayoutInfo::HBox] = createIconSet(QStringLiteral("edithlayout.png"));
        m_icons.layoutIcons[LayoutInfo::VBox] = createIconSet(QStringLiteral("editvlayout.png"));
        m_icons.layoutIcons[LayoutInfo::Grid] = createIconSet(QStringLiteral("editgrid.png"));
        m_icons.layoutIcons[LayoutInfo::Form] = createIconSet(QStringLiteral("editform.png"));
    }

    void ObjectInspectorModel::clearItems()
    {
        beginResetModel();
        m_objectIndexMultiMap.clear();
        m_model.clear();
        endResetModel(); // force editors to be closed in views
        removeRow(0);
    }

    ObjectInspectorModel::UpdateResult ObjectInspectorModel::update(QDesignerFormWindowInterface *fw)
    {
        QWidget *mainContainer = fw ? fw->mainContainer() : nullptr;
        if (!mainContainer) {
            clearItems();
            m_formWindow = nullptr;
            return NoForm;
        }
        m_formWindow = fw;
        // Build new model and compare to previous one. If the structure is
        // identical, just update, else rebuild
        ObjectModel newModel;

        static const QString separator = QCoreApplication::translate("ObjectInspectorModel", "separator");
        const ModelRecursionContext ctx(fw->core(),  separator);
        createModelRecursion(fw, nullptr, mainContainer, newModel, ctx);

        if (newModel == m_model) {
            updateItemContents(m_model, newModel);
            return Updated;
        }

        rebuild(newModel);
        m_model = newModel;
        return Rebuilt;
    }

    QObject *ObjectInspectorModel::objectAt(const QModelIndex &index) const
    {
        if (index.isValid())
            if (const QStandardItem *item = itemFromIndex(index))
                return objectOfItem(item);
        return nullptr;
    }

    // Missing Qt API: get a row
    ObjectInspectorModel::StandardItemList ObjectInspectorModel::rowAt(QModelIndex index) const
    {
        StandardItemList rc;
        while (true) {
            rc += itemFromIndex(index);
            const int nextColumn = index.column() + 1;
            if (nextColumn >=  NumColumns)
                break;
            index = index.sibling(index.row(), nextColumn);
        }
        return rc;
    }

    // Rebuild the tree in case the model has completely changed.
    void ObjectInspectorModel::rebuild(const ObjectModel &newModel)
    {
        clearItems();
        if (newModel.isEmpty())
            return;

        const ObjectModel::const_iterator mcend = newModel.constEnd();
        ObjectModel::const_iterator it = newModel.constBegin();
        // Set up root element
        StandardItemList rootRow = createModelRow(it->object());
        it->setItems(rootRow, m_icons);
        appendRow(rootRow);
        m_objectIndexMultiMap.insert(it->object(), indexFromItem(rootRow.constFirst()));
        for (++it; it != mcend; ++it) {
            // Add to parent item, found via map
            const QModelIndex parentIndex = m_objectIndexMultiMap.value(it->parent(), QModelIndex());
            Q_ASSERT(parentIndex.isValid());
            QStandardItem *parentItem = itemFromIndex(parentIndex);
            StandardItemList row = createModelRow(it->object());
            it->setItems(row, m_icons);
            parentItem->appendRow(row);
            m_objectIndexMultiMap.insert(it->object(), indexFromItem(row.constFirst()));
        }
    }

    // Update item data in case the model has the same structure
    void ObjectInspectorModel::updateItemContents(ObjectModel &oldModel, const ObjectModel &newModel)
    {
        // Change text and icon. Keep a set of changed object
        // as for example actions might occur several times in the tree.
        using QObjectSet = QSet<QObject *>;

        QObjectSet changedObjects;

        const int size = newModel.size();
        Q_ASSERT(oldModel.size() ==  size);
        for (int i = 0; i < size; i++) {
            const ObjectData &newEntry = newModel[i];
            ObjectData &entry =  oldModel[i];
            // Has some data changed?
            if (const unsigned changedMask = entry.compare(newEntry)) {
                entry = newEntry;
                QObject * o = entry.object();
                if (!changedObjects.contains(o)) {
                    changedObjects.insert(o);
                    const QModelIndexList indexes =  m_objectIndexMultiMap.values(o);
                    for (const QModelIndex &index : indexes)
                        entry.setItemsDisplayData(rowAt(index), m_icons, changedMask);
                }
            }
        }
    }

    QVariant ObjectInspectorModel::data(const QModelIndex &index, int role) const
    {
        const QVariant rc = QStandardItemModel::data(index, role);
        // Return <noname> if the string is empty for the display role
        // only (else, editing starts with <noname>).
        if (role == Qt::DisplayRole && rc.metaType().id() == QMetaType::QString) {
            const QString s = rc.toString();
            if (s.isEmpty()) {
                static const QString noName = QCoreApplication::translate("ObjectInspectorModel", "<noname>");
                return  QVariant(noName);
            }
        }
        return rc;
    }

    bool ObjectInspectorModel::setData(const QModelIndex &index, const QVariant &value, int role)
    {
        if (role != Qt::EditRole || !m_formWindow)
            return false;

        QObject *object = objectAt(index);
        if (!object)
            return false;
        // Is this a layout widget?
        const QString nameProperty = isQLayoutWidget(object) ? QStringLiteral("layoutName") : QStringLiteral("objectName");
        m_formWindow->commandHistory()->push(createTextPropertyCommand(nameProperty, value.toString(), object, m_formWindow));
        return true;
    }
}

QT_END_NAMESPACE
